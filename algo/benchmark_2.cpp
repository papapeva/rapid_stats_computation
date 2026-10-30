#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "floyd_rivest.hpp"
#include "random_sampling.hpp"
#include "approx_median.hpp"

namespace {

using Clock = std::chrono::steady_clock;
using Data = std::vector<int>;

constexpr std::uint64_t kSeed = 0xC0FFEEULL;
constexpr std::size_t kSampleH = 100000;
constexpr double kDelta = 0.1;

struct RankRequest {
    std::string label;
    std::size_t rank;
};

struct Measurement {
    std::string name;
    double seconds = 0.0;
    std::vector<int> values;
};

struct BenchmarkPoint {
    std::size_t n = 0;
    double seconds = 0.0;
    double mean_rank_error = 0.0;
};

struct AlgorithmSeries {
    std::string name;
    std::vector<BenchmarkPoint> points;
};

template <typename Function>
double measure_seconds(Function&& function) {
    const auto start = Clock::now();
    function();
    const auto finish = Clock::now();
    return std::chrono::duration<double>(finish - start).count();
}

Data make_data(std::size_t n) {
    Data data(n);
    std::mt19937_64 rng(kSeed);
    std::uniform_int_distribution<int> distribution(
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max());

    for (auto& value : data) {
        value = distribution(rng);
    }
    return data;
}

std::vector<RankRequest> make_ranks(std::size_t n) {
    if (n == 0) {
        throw std::invalid_argument("N must be greater than zero");
    }

    return {
        {"first (min)", 0},
        {"10%", (n - 1) / 10},
        {"50% (median)", (n - 1) / 2},
        {"90%", (n - 1) * 9 / 10},
        {"last (max)", n - 1}
    };
}

std::size_t lower_bound_rank(const Data& sorted, int value) {
    return static_cast<std::size_t>(
        std::lower_bound(sorted.begin(), sorted.end(), value) - sorted.begin());
}

Measurement run_floyd_rivest(
    const Data& original,
    const std::vector<std::size_t>& ranks)
{
    Data data = original;
    Measurement measurement;
    measurement.name = "Floyd-Rivest";
    measurement.values.resize(ranks.size());

    measurement.seconds = measure_seconds([&] {
        rapid_stats::FloydRivestSelect<Data> algorithm(data);
        const auto iterators = algorithm.sort(ranks);
        if (iterators.size() != ranks.size()) {
            throw std::runtime_error("Floyd-Rivest: wrong result size");
        }
        for (std::size_t i = 0; i < iterators.size(); ++i) {
            measurement.values[i] = *iterators[i];
        }
    });

    return measurement;
}

Measurement run_nth_element(
    const Data& original,
    const std::vector<std::size_t>& ranks)
{
    Measurement measurement;
    measurement.name = "std::nth_element (5 selections)";
    measurement.values.resize(ranks.size());

    // Each rank gets a fresh copy; copying is excluded from the timed interval.
    for (std::size_t i = 0; i < ranks.size(); ++i) {
        Data working = original;
        const auto elapsed = measure_seconds([&] {
            auto nth = working.begin() + static_cast<std::ptrdiff_t>(ranks[i]);
            std::nth_element(working.begin(), nth, working.end());
            measurement.values[i] = *nth;
        });
        measurement.seconds += elapsed;
    }

    return measurement;
}

template <typename Algorithm>
Measurement run_algorithm(
    const std::string& name,
    const Data& original,
    const std::vector<std::size_t>& ranks,
    Algorithm&& make_algorithm)
{
    Data data = original;
    Measurement measurement;
    measurement.name = name;
    measurement.values.resize(ranks.size());

    measurement.seconds = measure_seconds([&] {
        auto algorithm = make_algorithm(data);
        const auto iterators = algorithm.sort(ranks);
        if (iterators.size() != ranks.size()) {
            throw std::runtime_error(name + ": wrong result size");
        }
        for (std::size_t i = 0; i < iterators.size(); ++i) {
            measurement.values[i] = *iterators[i];
        }
    });

    return measurement;
}

std::vector<Measurement> run_all_algorithms(
    const Data& original,
    const std::vector<std::size_t>& ranks,
    std::size_t n)
{
    std::vector<Measurement> results;
    results.reserve(5);

    results.push_back(run_floyd_rivest(original, ranks));
    results.push_back(run_nth_element(original, ranks));

    results.push_back(run_algorithm(
        "RandomSampling + Floyd-Rivest",
        original, ranks,
        [](Data& data) {
            auto exact = [](Data& values, std::size_t k) -> int {
                if (values.empty() || k >= values.size()) {
                    throw std::out_of_range("k is out of range");
                }
                rapid_stats::detail::floyd_rivest_select(
                    values, 0, values.size() - 1, k,
                    [](const int& a, const int& b) { return a < b; });
                return values[k];
            };
            return rapid_stats::RandomSamplingAlgorithm<
                Data, decltype(exact)>(data, kSampleH, kDelta, kSeed, exact);
        }));

    results.push_back(run_algorithm(
        "RandomSampling + nth_element",
        original, ranks,
        [](Data& data) {
            auto exact = [](Data& values, std::size_t k) -> int {
                if (values.empty() || k >= values.size()) {
                    throw std::out_of_range("k is out of range");
                }
                auto nth = values.begin() + static_cast<std::ptrdiff_t>(k);
                std::nth_element(values.begin(), nth, values.end());
                return values[k];
            };
            return rapid_stats::RandomSamplingAlgorithm<
                Data, decltype(exact)>(data, kSampleH, kDelta, kSeed, exact);
        }));

    results.push_back(run_algorithm(
        "ApproxMedianAlgorithm",
        original, ranks,
        [](Data& data) {
            return rapid_stats::ApproxMedianAlgorithm<Data>(
                data, 0.01, 1.0, kSeed);
        }));

    (void)n;
    return results;
}

double mean_absolute_rank_error(
    const Measurement& measurement,
    const Data& sorted_reference,
    const std::vector<RankRequest>& requests)
{
    double total = 0.0;

    for (std::size_t i = 0; i < requests.size(); ++i) {
        const std::size_t target_rank = requests[i].rank;
        const std::size_t actual_rank =
            lower_bound_rank(sorted_reference, measurement.values[i]);

        const std::size_t error = actual_rank > target_rank
            ? actual_rank - target_rank
            : target_rank - actual_rank;

        total += static_cast<double>(error);
    }

    return requests.empty() ? 0.0 : total / requests.size();
}

void print_quality(
    const Measurement& measurement,
    const Data& sorted_reference,
    const std::vector<RankRequest>& requests)
{
    std::cout << "\nQuality: " << measurement.name << '\n';

    for (std::size_t i = 0; i < requests.size(); ++i) {
        const auto target_rank = requests[i].rank;
        const int actual_value = measurement.values[i];
        const std::size_t actual_rank =
            lower_bound_rank(sorted_reference, actual_value);

        const auto rank_error = actual_rank > target_rank
            ? actual_rank - target_rank
            : target_rank - actual_rank;

        std::cout << "  " << std::left << std::setw(14) << requests[i].label
                  << " target=" << std::setw(12) << target_rank
                  << " value=" << std::setw(12) << actual_value
                  << " actual_rank=" << std::setw(12) << actual_rank
                  << " abs_rank_error=" << rank_error
                  << (actual_value == sorted_reference[target_rank]
                      ? " [exact value]" : "")
                  << '\n';
    }
}

std::string xml_escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

enum class Metric { Time, Error };

void save_svg_line_chart(
    const std::string& filename,
    const std::string& title,
    const std::string& y_label,
    const std::vector<AlgorithmSeries>& series,
    Metric metric)
{
    constexpr int width = 1250;
    constexpr int height = 720;
    constexpr int left = 105;
    constexpr int right = 35;
    constexpr int top = 75;
    constexpr int bottom = 155;
    const int plot_width = width - left - right;
    const int plot_height = height - top - bottom;

    if (series.empty()) {
        throw std::runtime_error("No data for chart: " + filename);
    }

    std::size_t count = 0;
    double max_y = 0.0;
    for (const auto& algorithm : series) {
        count = std::max(count, algorithm.points.size());
        for (const auto& point : algorithm.points) {
            const double value =
                metric == Metric::Time ? point.seconds : point.mean_rank_error;
            max_y = std::max(max_y, value);
        }
    }

    if (count == 0) {
        throw std::runtime_error("No points for chart: " + filename);
    }
    if (max_y <= 0.0) {
        max_y = 1.0;
    } else {
        max_y *= 1.10;
    }

    std::ofstream svg(filename);
    if (!svg) {
        throw std::runtime_error("Cannot create " + filename);
    }

    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
        << width << "\" height=\"" << height << "\" viewBox=\"0 0 "
        << width << ' ' << height << "\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    svg << "<text x=\"" << width / 2
        << "\" y=\"35\" text-anchor=\"middle\" font-family=\"Arial\" "
        << "font-size=\"23\" font-weight=\"bold\">"
        << xml_escape(title) << "</text>\n";

    // Numeric N is plotted on a logarithmic X axis, so 100k, 1M and 10M
    // are spaced according to their actual scale rather than evenly.
    double min_log_n = std::numeric_limits<double>::infinity();
    double max_log_n = -std::numeric_limits<double>::infinity();
    for (const auto& algorithm : series) {
        for (const auto& point : algorithm.points) {
            const double ln = std::log10(static_cast<double>(point.n));
            min_log_n = std::min(min_log_n, ln);
            max_log_n = std::max(max_log_n, ln);
        }
    }
    if (std::abs(max_log_n - min_log_n) < 1e-12) {
        min_log_n -= 0.5;
        max_log_n += 0.5;
    }

    auto x_coord = [&](std::size_t n) {
        const double t =
            (std::log10(static_cast<double>(n)) - min_log_n) /
            (max_log_n - min_log_n);
        return left + t * plot_width;
    };
    auto y_coord = [&](double value) {
        return top + plot_height - value / max_y * plot_height;
    };

    svg << "<line x1=\"" << left << "\" y1=\"" << top
        << "\" x2=\"" << left << "\" y2=\"" << top + plot_height
        << "\" stroke=\"black\" stroke-width=\"2\"/>\n";
    svg << "<line x1=\"" << left << "\" y1=\"" << top + plot_height
        << "\" x2=\"" << left + plot_width << "\" y2=\""
        << top + plot_height << "\" stroke=\"black\" stroke-width=\"2\"/>\n";

    svg << std::fixed << std::setprecision(3);
    constexpr int ticks = 5;
    for (int i = 0; i <= ticks; ++i) {
        const double value = max_y * i / ticks;
        const double y = y_coord(value);
        svg << "<line x1=\"" << left << "\" y1=\"" << y
            << "\" x2=\"" << left + plot_width << "\" y2=\"" << y
            << "\" stroke=\"#dddddd\"/>\n";
        svg << "<text x=\"" << left - 10 << "\" y=\"" << y + 5
            << "\" text-anchor=\"end\" font-family=\"Arial\" "
            << "font-size=\"13\">" << value << "</text>\n";
    }

    svg << "<text x=\"" << left + plot_width / 2
        << "\" y=\"" << height - 95
        << "\" text-anchor=\"middle\" font-family=\"Arial\" "
        << "font-size=\"16\">Input size N (logarithmic scale)</text>\n";
    svg << "<text x=\"25\" y=\"" << top + plot_height / 2
        << "\" transform=\"rotate(-90 25 " << top + plot_height / 2
        << ")\" text-anchor=\"middle\" font-family=\"Arial\" "
        << "font-size=\"16\">" << xml_escape(y_label) << "</text>\n";

    // Show each unique N on the X axis.
    std::vector<std::size_t> sizes;
    for (const auto& algorithm : series) {
        for (const auto& point : algorithm.points) {
            sizes.push_back(point.n);
        }
    }
    std::sort(sizes.begin(), sizes.end());
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

    for (std::size_t n : sizes) {
        const double x = x_coord(n);
        svg << "<line x1=\"" << x << "\" y1=\"" << top + plot_height
            << "\" x2=\"" << x << "\" y2=\"" << top + plot_height + 6
            << "\" stroke=\"black\"/>\n";
        svg << "<text x=\"" << x << "\" y=\"" << top + plot_height + 25
            << "\" text-anchor=\"middle\" font-family=\"Arial\" "
            << "font-size=\"12\">" << n << "</text>\n";
    }

    const std::vector<std::string> colors = {
        "#2563eb", "#dc2626", "#16a34a", "#9333ea", "#ea580c"
    };

    for (std::size_t s = 0; s < series.size(); ++s) {
        const auto& algorithm = series[s];
        const std::string& color = colors[s % colors.size()];

        if (algorithm.points.empty()) {
            continue;
        }

        svg << "<polyline fill=\"none\" stroke=\"" << color
            << "\" stroke-width=\"3\" points=\"";
        for (const auto& point : algorithm.points) {
            const double value =
                metric == Metric::Time ? point.seconds : point.mean_rank_error;
            svg << x_coord(point.n) << ',' << y_coord(value) << ' ';
        }
        svg << "\"/>\n";

        for (const auto& point : algorithm.points) {
            const double value =
                metric == Metric::Time ? point.seconds : point.mean_rank_error;
            svg << "<circle cx=\"" << x_coord(point.n)
                << "\" cy=\"" << y_coord(value)
                << "\" r=\"5\" fill=\"" << color << "\"/>\n";
        }
    }

    // Legend in rows to avoid overlap with long algorithm names.
    const int legend_start_y = height - 58;
    for (std::size_t s = 0; s < series.size(); ++s) {
        const int col = static_cast<int>(s % 2);
        const int row = static_cast<int>(s / 2);
        const int x = 90 + col * 570;
        const int y = legend_start_y + row * 22;

        svg << "<line x1=\"" << x << "\" y1=\"" << y
            << "\" x2=\"" << x + 25 << "\" y2=\"" << y
            << "\" stroke=\"" << colors[s % colors.size()]
            << "\" stroke-width=\"3\"/>\n";
        svg << "<text x=\"" << x + 32 << "\" y=\"" << y + 5
            << "\" font-family=\"Arial\" font-size=\"12\">"
            << xml_escape(series[s].name) << "</text>\n";
    }

    svg << "</svg>\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0]
                      << " N1 [N2 ...]\n"
                      << "Example: " << argv[0]
                      << " 100000 1000000 10000000\n";
            return 1;
        }

        std::vector<std::size_t> sizes;
        sizes.reserve(static_cast<std::size_t>(argc - 1));

        for (int i = 1; i < argc; ++i) {
            std::size_t parsed_chars = 0;
            const unsigned long long parsed =
                std::stoull(argv[i], &parsed_chars);

            if (parsed_chars != std::string(argv[i]).size() ||
                parsed == 0 ||
                parsed > std::numeric_limits<std::size_t>::max()) {
                throw std::invalid_argument(
                    "Invalid positive array size: " + std::string(argv[i]));
            }

            sizes.push_back(static_cast<std::size_t>(parsed));
        }

        std::sort(sizes.begin(), sizes.end());
        sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

        AlgorithmSeries floyd{"Floyd-Rivest", {}};
        AlgorithmSeries nth{"std::nth_element (5 selections)", {}};
        AlgorithmSeries sampling_fr{"RandomSampling + Floyd-Rivest", {}};
        AlgorithmSeries sampling_nth{"RandomSampling + nth_element", {}};
        AlgorithmSeries approx{"ApproxMedianAlgorithm", {}};

        for (const std::size_t n : sizes) {
            std::cout << "\n========================================\n"
                      << "Benchmark N = " << n << '\n'
                      << "========================================\n";

            const auto requests = make_ranks(n);
            std::vector<std::size_t> ranks;
            ranks.reserve(requests.size());
            for (const auto& request : requests) {
                ranks.push_back(request.rank);
            }

            std::cout << "Generating input data...\n";
            const Data original = make_data(n);

            std::cout << "Building sorted reference for rank-error measurement...\n";
            Data sorted_reference = original;
            const double reference_seconds = measure_seconds([&] {
                std::sort(sorted_reference.begin(), sorted_reference.end());
            });
            std::cout << "Reference std::sort: " << std::fixed
                      << std::setprecision(3) << reference_seconds << " s\n";

            auto results = run_all_algorithms(original, ranks, n);

            for (const auto& result : results) {
                std::cout << std::left << std::setw(42) << result.name
                          << std::right << std::fixed << std::setprecision(3)
                          << result.seconds << " s\n";
                print_quality(result, sorted_reference, requests);
            }

            auto add_point = [&](AlgorithmSeries& target,
                                 const Measurement& measurement) {
                target.points.push_back(BenchmarkPoint{
                    n,
                    measurement.seconds,
                    mean_absolute_rank_error(
                        measurement, sorted_reference, requests)
                });
            };

            // Exact methods.
            add_point(floyd, results[0]);
            add_point(nth, results[1]);

            // RandomSampling is treated as an approximate algorithm; the
            // selector used internally is exact, but the overall sampling
            // algorithm's result quality is measured empirically.
            add_point(sampling_fr, results[2]);
            add_point(sampling_nth, results[3]);
            add_point(approx, results[4]);
        }

        const std::vector<AlgorithmSeries> exact_times = {
            floyd, nth
        };
        const std::vector<AlgorithmSeries> approximate_times = {
            sampling_fr, sampling_nth, approx
        };
        const std::vector<AlgorithmSeries> approximate_errors = {
            sampling_fr, sampling_nth, approx
        };

        save_svg_line_chart(
            "exact_times.svg",
            "Execution Time - Exact Selection Methods",
            "Time (seconds)",
            exact_times,
            Metric::Time);

        save_svg_line_chart(
            "approx_times.svg",
            "Execution Time - Approximate Methods",
            "Time (seconds)",
            approximate_times,
            Metric::Time);

        save_svg_line_chart(
            "approx_errors.svg",
            "Mean Absolute Rank Error - Approximate Methods",
            "Mean absolute rank error (positions)",
            approximate_errors,
            Metric::Error);

        std::cout << "\nCreated SVG charts:\n"
                  << "  exact_times.svg\n"
                  << "  approx_times.svg\n"
                  << "  approx_errors.svg\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}

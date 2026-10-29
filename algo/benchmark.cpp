#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "floyd_rivest.hpp"
#include "random_sampling.hpp"
#include "standard_select.hpp"

using Value = std::int64_t;
using Clock = std::chrono::steady_clock;

constexpr int kRepetitions = 3;
constexpr double kDelta = 0.1;
constexpr std::uint64_t kDataSeed = 42;
constexpr std::uint64_t kSampleSeed = 12345;

struct PlotResult {
    std::size_t n;
    std::string position;
    double floyd_ms;
    double sampling_fr_ms;
    double sampling_nth_ms;
    double nth_ms;
};

struct Measurement {
    double median_ms;
    Value result;
};

template <typename Func>
Measurement measure_median_ms(
    const std::vector<Value>& original,
    std::size_t k,
    Func select
) {
    std::array<double, kRepetitions> times{};
    Value result{};

    for (int trial = 0; trial < kRepetitions; ++trial) {
        // Copying is deliberately outside the timed interval because the
        // benchmark measures selection itself, not input preparation.
        std::vector<Value> work = original;
        const auto start = Clock::now();
        result = select(work, k);
        const auto finish = Clock::now();
        times[static_cast<std::size_t>(trial)] =
            std::chrono::duration<double, std::milli>(finish - start).count();
    }

    std::sort(times.begin(), times.end());
    return {times[times.size() / 2], result};
}

std::vector<Value> generate_data(std::size_t n, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    // Limited range intentionally includes duplicate values.
    std::uniform_int_distribution<Value> dist(0, 1'000'000);
    std::vector<Value> data(n);
    for (auto& x : data) {
        x = dist(rng);
    }
    return data;
}

// Check that value belongs to the original array and at least one of its
// 1-based ranks lies in [max(1, k+1-h), min(n, k+1+h)].
bool rank_is_acceptable(
    const std::vector<Value>& sorted,
    Value value,
    std::size_t k,
    std::size_t h
) {
    const std::size_t n = sorted.size();
    const std::size_t requested_rank = k + 1;
    const std::size_t a = (k >= h) ? (k - h + 1) : 1;
    const std::size_t remaining = n - requested_rank;
    const std::size_t b = (h > remaining) ? n : requested_rank + h;

    const auto lower = std::lower_bound(sorted.begin(), sorted.end(), value);
    const auto upper = std::upper_bound(sorted.begin(), sorted.end(), value);
    if (lower == sorted.end() || *lower != value) {
        return false;
    }

    const std::size_t first_rank =
        static_cast<std::size_t>(lower - sorted.begin()) + 1;
    const std::size_t last_rank =
        static_cast<std::size_t>(upper - sorted.begin());
    return first_rank <= b && last_rank >= a;
}


std::size_t rank_error(
    const std::vector<Value>& sorted,
    Value value,
    std::size_t k
) {
    // k is zero-based, while statistical ranks are 1-based.
    const std::size_t target_rank = k + 1;

    const auto lower = std::lower_bound(
        sorted.begin(), sorted.end(), value);
    const auto upper = std::upper_bound(
        sorted.begin(), sorted.end(), value);

    if (lower == sorted.end() || *lower != value) {
        throw std::runtime_error(
            "Selected value does not belong to the array");
    }

    const std::size_t first_rank =
        static_cast<std::size_t>(lower - sorted.begin()) + 1;
    const std::size_t last_rank =
        static_cast<std::size_t>(upper - sorted.begin());

    if (target_rank < first_rank) {
        return first_rank - target_rank;
    }

    if (target_rank > last_rank) {
        return target_rank - last_rank;
    }

    return 0;
}


void save_benchmark_plot(
    const std::vector<PlotResult>& results,
    const std::string& filename = "benchmark_results.svg"
) {
    if (results.empty()) {
        throw std::invalid_argument("No benchmark results to plot");
    }

    const std::array<std::string, 5> positions = {
        "Minimum", "10%", "Median", "90%", "Maximum"
    };
    const std::array<std::string, 4> names = {
        "Floyd-Rivest",
        "Random Sampling + FR fallback",
        "Random Sampling + nth fallback",
        "std::nth_element"
    };
    const std::array<std::string, 4> colors = {
        "#2563eb", "#16a34a", "#d97706", "#dc2626"
    };

    constexpr int width = 1200;
    constexpr int panel_height = 245;
    constexpr int left = 95;
    constexpr int right = 35;
    constexpr int top = 40;
    constexpr int bottom = 48;
    const int height = 70 + static_cast<int>(positions.size()) * panel_height;

    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("Cannot create SVG file: " + filename);
    }
    out << std::fixed << std::setprecision(2);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
        << "\" height=\"" << height << "\" viewBox=\"0 0 "
        << width << " " << height << "\">\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    out << "<text x=\"" << width / 2
        << "\" y=\"28\" text-anchor=\"middle\" font-family=\"sans-serif\" "
        << "font-size=\"21\" font-weight=\"bold\">Selection algorithms benchmark "
        << "(median of " << kRepetitions << " runs)</text>\n";

    const std::array<int, 4> legend_x = {55, 330, 655, 950};
    for (std::size_t i = 0; i < names.size(); ++i) {
        out << "<line x1=\"" << legend_x[i] << "\" y1=\"52\" x2=\""
            << legend_x[i] + 22 << "\" y2=\"52\" stroke=\"" << colors[i]
            << "\" stroke-width=\"3\"/>\n";
        out << "<text x=\"" << legend_x[i] + 28
            << "\" y=\"57\" font-family=\"sans-serif\" font-size=\"12\">"
            << names[i] << "</text>\n";
    }

    std::vector<std::size_t> sizes;
    for (const auto& r : results) {
        if (r.n > 0) sizes.push_back(r.n);
    }
    std::sort(sizes.begin(), sizes.end());
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
    if (sizes.empty()) {
        throw std::invalid_argument("No valid array sizes to plot");
    }

    const double min_log = std::log10(static_cast<double>(sizes.front()));
    const double max_log = std::log10(static_cast<double>(sizes.back()));
    const auto x_for_n = [&](std::size_t n, int plot_left, int plot_right) {
        if (sizes.size() == 1 || max_log == min_log) {
            return (plot_left + plot_right) / 2.0;
        }
        return plot_left + (plot_right - plot_left) *
            (std::log10(static_cast<double>(n)) - min_log) / (max_log - min_log);
    };

    for (std::size_t p = 0; p < positions.size(); ++p) {
        const int panel_y = 70 + static_cast<int>(p) * panel_height;
        const int plot_top = panel_y + top;
        const int plot_bottom = panel_y + panel_height - bottom;
        const int plot_left = left;
        const int plot_right = width - right;
        double max_time = 0.0;

        for (const auto& r : results) {
            if (r.position == positions[p]) {
                max_time = std::max({max_time, r.floyd_ms, r.sampling_fr_ms,
                                     r.sampling_nth_ms, r.nth_ms});
            }
        }
        if (max_time <= 0.0) continue;
        max_time *= 1.10;

        out << "<text x=\"" << plot_left << "\" y=\"" << panel_y + 20
            << "\" font-family=\"sans-serif\" font-size=\"15\" font-weight=\"bold\">"
            << positions[p] << "</text>\n";

        for (int tick = 0; tick <= 4; ++tick) {
            const double value = max_time * tick / 4.0;
            const int y = plot_bottom - (plot_bottom - plot_top) * tick / 4;
            out << "<line x1=\"" << plot_left << "\" y1=\"" << y
                << "\" x2=\"" << plot_right << "\" y2=\"" << y
                << "\" stroke=\"#e5e7eb\"/>\n";
            out << "<text x=\"" << plot_left - 9 << "\" y=\"" << y + 4
                << "\" text-anchor=\"end\" font-family=\"sans-serif\" font-size=\"10\">"
                << value << "</text>\n";
        }
        out << "<line x1=\"" << plot_left << "\" y1=\"" << plot_top
            << "\" x2=\"" << plot_left << "\" y2=\"" << plot_bottom
            << "\" stroke=\"#374151\"/>\n";
        out << "<line x1=\"" << plot_left << "\" y1=\"" << plot_bottom
            << "\" x2=\"" << plot_right << "\" y2=\"" << plot_bottom
            << "\" stroke=\"#374151\"/>\n";
        out << "<text transform=\"translate(24," << (plot_top + plot_bottom) / 2
            << ") rotate(-90)\" text-anchor=\"middle\" font-family=\"sans-serif\" "
            << "font-size=\"12\">Time (ms)</text>\n";
        out << "<text x=\"" << (plot_left + plot_right) / 2 << "\" y=\""
            << plot_bottom + 36
            << "\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"11\">"
            << "Array size N (log scale)</text>\n";

        for (std::size_t i = 0; i < sizes.size(); ++i) {
            out << "<text x=\"" << x_for_n(sizes[i], plot_left, plot_right)
                << "\" y=\"" << plot_bottom + 15
                << "\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"10\">"
                << sizes[i] << "</text>\n";
        }

        for (int algorithm = 0; algorithm < 4; ++algorithm) {
            std::ostringstream points;
            bool first = true;
            for (std::size_t n : sizes) {
                const PlotResult* found = nullptr;
                for (const auto& r : results) {
                    if (r.n == n && r.position == positions[p]) {
                        found = &r;
                        break;
                    }
                }
                if (!found) continue;
                const double value = algorithm == 0 ? found->floyd_ms
                    : algorithm == 1 ? found->sampling_fr_ms
                    : algorithm == 2 ? found->sampling_nth_ms : found->nth_ms;
                const double x = x_for_n(n, plot_left, plot_right);
                const double y = plot_bottom -
                    (plot_bottom - plot_top) * value / max_time;
                if (!first) points << ' ';
                points << x << ',' << y;
                first = false;
                out << "<circle cx=\"" << x << "\" cy=\"" << y
                    << "\" r=\"3.5\" fill=\"" << colors[algorithm] << "\"/>\n";
            }
            out << "<polyline fill=\"none\" stroke=\"" << colors[algorithm]
                << "\" stroke-width=\"2.2\" points=\"" << points.str() << "\"/>\n";
        }
    }
    out << "</svg>\n";
    if (!out) {
        throw std::runtime_error("Failed to write SVG file: " + filename);
    }
}

void run_test(
    const std::vector<Value>& original,
    const std::string& label,
    double k_fraction,
    std::size_t h,
    std::uint64_t sample_seed,
    std::vector<PlotResult>& plot_results
) {
    const std::size_t n = original.size();
    const std::size_t k = static_cast<std::size_t>((n - 1) * k_fraction);
    std::vector<Value> sorted = original;
    std::sort(sorted.begin(), sorted.end());

    const auto fr = measure_median_ms(original, k,
        [](std::vector<Value>& a, std::size_t kk) {
            return floyd_rivest_select(a, kk);
        });
    const auto rs_fr = measure_median_ms(original, k,
        [=](std::vector<Value>& a, std::size_t kk) {
            return random_sampling_select(
                a, kk, h, kDelta, sample_seed, floyd_rivest_select<Value>);
        });
    const auto rs_nth = measure_median_ms(original, k,
        [=](std::vector<Value>& a, std::size_t kk) {
            return random_sampling_select(
                a, kk, h, kDelta, sample_seed, standard_select<Value>);
        });
    const auto nth = measure_median_ms(original, k,
        [](std::vector<Value>& a, std::size_t kk) {
            return standard_select(a, kk);
        });

    const bool fr_ok = fr.result == sorted[k];
    const bool nth_ok = nth.result == sorted[k];
    const bool rs_fr_ok = rank_is_acceptable(sorted, rs_fr.result, k, h);
    const bool rs_nth_ok = rank_is_acceptable(sorted, rs_nth.result, k, h);
    if (!fr_ok || !nth_ok || !rs_fr_ok || !rs_nth_ok) {
        std::cerr << "Validation failed: position=" << label << ", n=" << n
                  << ", k=" << k << '\n'
                  << "Floyd-Rivest exact: " << fr_ok << '\n'
                  << "Random Sampling + FR fallback: " << rs_fr_ok << '\n'
                  << "Random Sampling + nth fallback: " << rs_nth_ok << '\n'
                  << "std::nth_element exact: " << nth_ok << '\n';
        throw std::runtime_error("Algorithm validation failed");
    }
    const std::size_t fr_error =
        rank_error(sorted, fr.result, k);

    const std::size_t rs_fr_error =
        rank_error(sorted, rs_fr.result, k);

    const std::size_t rs_nth_error =
        rank_error(sorted, rs_nth.result, k);

    const std::size_t nth_error =
        rank_error(sorted, nth.result, k);

    std::cout << std::left << std::setw(9) << label
              << std::right << std::setw(11) << n
              << std::setw(11) << k
              << std::setw(13) << fr.median_ms
              << std::setw(17) << rs_fr.median_ms
              << std::setw(17) << rs_nth.median_ms
              << std::setw(14) << nth.median_ms
              << std::setw(12) << fr_error
              << std::setw(12) << rs_fr_error
              << std::setw(12) << rs_nth_error
              << std::setw(12) << nth_error
              << "   OK\n";
    
    

    plot_results.push_back({n, label, fr.median_ms, rs_fr.median_ms,
                            rs_nth.median_ms, nth.median_ms});
}

int main(int argc, char* argv[]) {
    std::vector<std::size_t> sizes;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            const auto parsed = std::stoull(argv[i]);
            if (parsed == 0) {
                std::cerr << "Array sizes must be positive\n";
                return 1;
            }
            sizes.push_back(static_cast<std::size_t>(parsed));
        }
    } else {
        sizes = {1000000, 10000000, 100000000};
    }

    std::vector<PlotResult> plot_results;
    try {
        for (const std::size_t n : sizes) {
            const auto data = generate_data(n, kDataSeed);
            // h proportional to sqrt(n) keeps the sample substantially smaller
            // than the original array for the default sizes.
            const std::size_t h = static_cast<std::size_t>(
                std::ceil(8.0 * std::sqrt(static_cast<double>(n))));

            const long double epsilon = static_cast<long double>(h) / n;
            const long double raw_m =
                (std::log(2.0L) - std::log(static_cast<long double>(kDelta))) /
                (2.0L * epsilon * epsilon);
            const long double sample_size = std::ceil(raw_m);

            std::cout << "\nN=" << n << " | h=" << h
                      << " | delta=" << kDelta
                      << " | calculated sample m=";
            if (sample_size >= static_cast<long double>(n)) {
                std::cout << "fallback (m >= n)\n";
            } else {
                std::cout << static_cast<std::size_t>(sample_size) << '\n';
            }
            std::cout << std::left << std::setw(9) << "Position"
                      << std::right << std::setw(11) << "N"
                      << std::setw(11) << "k (0-base)"
                      << std::setw(13) << "FR ms"
                      << std::setw(17) << "RS + FR ms"
                      << std::setw(17) << "RS + nth ms"
                      << std::setw(14) << "nth ms"
                      << std::setw(12) << "FR error"
                      << std::setw(12) << "RS+FR err"
                      << std::setw(12) << "RS+nth err"
                      << std::setw(12) << "nth error"
                      << "   Check\n";

            run_test(data, "Minimum", 0.0, h, kSampleSeed, plot_results);
            run_test(data, "10%", 0.1, h, kSampleSeed, plot_results);
            run_test(data, "Median", 0.5, h, kSampleSeed, plot_results);
            run_test(data, "90%", 0.9, h, kSampleSeed, plot_results);
            run_test(data, "Maximum", 1.0, h, kSampleSeed, plot_results);
        }
        save_benchmark_plot(plot_results);
        std::cout << "\nPlot saved to benchmark_results.svg\n";
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << '\n';
        return 1;
    }
    return 0;
}

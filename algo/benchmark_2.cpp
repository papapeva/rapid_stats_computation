// g++ -std=c++17 -O2 -Wall -Wextra benchmark_2.cpp -o benchmark
// ./benchmark.exe
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "floyd_rivest.hpp"
#include "random_sampling.hpp"
#include "standard_select.hpp"
#include "approx_median.hpp"

// ============================================================
// 1. GLOBAL SETTINGS
// ============================================================

using Value = std::int64_t;
using Clock = std::chrono::steady_clock;

constexpr int kRepetitions = 3;
constexpr double kDelta = 0.1;
constexpr std::uint64_t kDataSeed = 42;
constexpr std::uint64_t kSampleSeed = 12345;

constexpr int kPlotWidth = 1200;
constexpr int kPanelHeight = 245;
constexpr int kPlotLeft = 95;
constexpr int kPlotRight = 35;
constexpr int kPlotTop = 40;
constexpr int kPlotBottom = 48;

// ============================================================
// 2. ALGORITHM INTERFACE
// ============================================================

// Every algorithm must implement this interface.
//
// Parameters:
//   array - array to process
//   k     - zero-based target index
//   h     - allowed rank error / sampling parameter
//   seed  - random seed
//
// Return:
//   selected value
//
// For algorithms that do not need h or seed, simply ignore them.

using SelectFunction = std::function<Value(
    std::vector<Value>&,
    std::size_t,
    std::size_t,
    std::uint64_t
)>;

struct Algorithm {
    std::string name;
    std::string color;
    SelectFunction select;

    // true  = must return the exact k-th element
    // false = approximate selection is allowed
    bool exact = true;
};

// ============================================================
// 3. ALGORITHM REGISTRY
// ============================================================
//
// ADD NEW ALGORITHMS HERE.
//
// In the common case, adding an algorithm requires one entry.
// No changes to benchmarking, statistics, tables or plotting.
//
// The four existing algorithms are registered below.

std::vector<Algorithm> create_algorithms() {
    return {
        {
            "Floyd-Rivest",
            "#2563eb",
            [](auto& a, std::size_t k, std::size_t, std::uint64_t) {
                return floyd_rivest_select(a, k);
            },
            true
        },

        {
            "Random Sampling + FR fallback",
            "#16a34a",
            [](auto& a, std::size_t k, std::size_t h,
               std::uint64_t seed) {
                return random_sampling_select(
                    a, k, h, kDelta, seed,
                    floyd_rivest_select<Value>
                );
            },
            false
        },

        {
            "Random Sampling + nth fallback",
            "#d97706",
            [](auto& a, std::size_t k, std::size_t h,
               std::uint64_t seed) {
                return random_sampling_select(
                    a, k, h, kDelta, seed,
                    standard_select<Value>
                );
            },
            false
        },

        {
            "std::nth_element",
            "#dc2626",
            [](auto& a, std::size_t k, std::size_t,
               std::uint64_t) {
                return standard_select(a, k);
            },
            true
        },
        {
            "Approx median",
            "#9333ea",
            [](auto& a, std::size_t k, std::size_t h,
            std::uint64_t seed) {
                return approximate_order_statistic(
                    a, k, h, 0.05, seed
                );
            },
            false
        }
    };
}

// ============================================================
// 4. DATA STRUCTURES
// ============================================================

struct Measurement {
    double median_ms;
    Value result;
};

struct AlgorithmResult {
    std::string name;
    std::string color;

    double time_ms = 0.0;
    std::size_t rank_error = 0;

    bool valid = false;
};

struct PlotResult {
    std::size_t n;
    std::string position;

    std::vector<AlgorithmResult> algorithms;
};

struct TestPosition {
    std::string name;
    double fraction;
};

const std::vector<TestPosition> kPositions = {
    {"Minimum", 0.0},
    {"10%",     0.1},
    {"Median",  0.5},
    {"90%",     0.9},
    {"Maximum", 1.0}
};

// ============================================================
// 5. BENCHMARKING
// ============================================================

template <typename Func>
Measurement measure_median_ms(
    const std::vector<Value>& original,
    std::size_t k,
    Func select
) {
    std::array<double, kRepetitions> times{};
    Value result{};

    for (int trial = 0; trial < kRepetitions; ++trial) {
        // Copying is deliberately excluded from the timed interval.
        std::vector<Value> work = original;

        const auto start = Clock::now();

        result = select(work, k);

        const auto finish = Clock::now();

        times[static_cast<std::size_t>(trial)] =
            std::chrono::duration<double, std::milli>(
                finish - start
            ).count();
    }

    std::sort(times.begin(), times.end());

    return {
        times[times.size() / 2],
        result
    };
}

// ============================================================
// 6. DATA GENERATION
// ============================================================

std::vector<Value> generate_data(
    std::size_t n,
    std::uint64_t seed
) {
    std::mt19937_64 rng(seed);

    // The limited range intentionally creates duplicate values.
    std::uniform_int_distribution<Value> dist(0, 1'000'000);

    std::vector<Value> data(n);

    for (auto& x : data) {
        x = dist(rng);
    }

    return data;
}

// ============================================================
// 7. VALIDATION AND STATISTICS
// ============================================================

// Check whether the selected value belongs to an acceptable rank
// interval around the requested zero-based index k.
//
// The interval is [max(1, k+1-h), min(n, k+1+h)]
// when expressed using one-based ranks.

bool rank_is_acceptable(
    const std::vector<Value>& sorted,
    Value value,
    std::size_t k,
    std::size_t h
) {
    const std::size_t n = sorted.size();
    const std::size_t requested_rank = k + 1;

    const std::size_t a =
        (k >= h) ? (k - h + 1) : 1;

    const std::size_t remaining = n - requested_rank;

    const std::size_t b =
        (h > remaining) ? n : requested_rank + h;

    const auto lower = std::lower_bound(
        sorted.begin(), sorted.end(), value
    );

    const auto upper = std::upper_bound(
        sorted.begin(), sorted.end(), value
    );

    if (lower == sorted.end() || *lower != value) {
        return false;
    }

    const std::size_t first_rank =
        static_cast<std::size_t>(lower - sorted.begin()) + 1;

    const std::size_t last_rank =
        static_cast<std::size_t>(upper - sorted.begin());

    return first_rank <= b && last_rank >= a;
}

// Compute distance from the requested rank to the selected value's
// range of possible ranks. Duplicate values are handled correctly.

std::size_t rank_error(
    const std::vector<Value>& sorted,
    Value value,
    std::size_t k
) {
    const std::size_t target_rank = k + 1;

    const auto lower = std::lower_bound(
        sorted.begin(), sorted.end(), value
    );

    const auto upper = std::upper_bound(
        sorted.begin(), sorted.end(), value
    );

    if (lower == sorted.end() || *lower != value) {
        throw std::runtime_error(
            "Selected value does not belong to the array"
        );
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

// ============================================================
// 8. CONSOLE OUTPUT
// ============================================================

void print_table_header(
    const std::vector<Algorithm>& algorithms
) {
    std::cout
        << std::left
        << std::setw(12) << "Position"
        << std::right
        << std::setw(12) << "N"
        << std::setw(12) << "k (0-base)";

    for (const auto& algorithm : algorithms) {
        std::cout
            << std::setw(18)
            << (algorithm.name + " ms");
    }

    for (const auto& algorithm : algorithms) {
        std::cout
            << std::setw(18)
            << (algorithm.name + " err");
    }

    std::cout << "   Check\n";
}

void print_table_row(
    const std::string& position,
    std::size_t n,
    std::size_t k,
    const std::vector<AlgorithmResult>& results
) {
    std::cout
        << std::left
        << std::setw(12) << position
        << std::right
        << std::setw(12) << n
        << std::setw(12) << k;

    for (const auto& result : results) {
        std::cout
            << std::setw(18)
            << result.time_ms;
    }

    for (const auto& result : results) {
        std::cout
            << std::setw(18)
            << result.rank_error;
    }

    const bool all_valid = std::all_of(
        results.begin(),
        results.end(),
        [](const AlgorithmResult& r) {
            return r.valid;
        }
    );

    std::cout << "   "
              << (all_valid ? "OK" : "FAILED")
              << '\n';
}

// ============================================================
// 9. SVG HELPERS
// ============================================================

std::string xml_escape(const std::string& text) {
    std::string result;

    for (const char c : text) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '<':  result += "&lt;";   break;
            case '>':  result += "&gt;";   break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c;        break;
        }
    }

    return result;
}

double x_for_n(
    std::size_t n,
    std::size_t min_n,
    std::size_t max_n,
    int plot_left,
    int plot_right
) {
    const double min_log =
        std::log10(static_cast<double>(min_n));

    const double max_log =
        std::log10(static_cast<double>(max_n));

    if (min_n == max_n || max_log == min_log) {
        return (plot_left + plot_right) / 2.0;
    }

    return plot_left +
        (plot_right - plot_left) *
        (std::log10(static_cast<double>(n)) - min_log) /
        (max_log - min_log);
}

// ============================================================
// 10. GENERIC SVG PLOTTER
// ============================================================
//
// The plotter does not know any algorithm names in advance.
// It discovers algorithms from the registry and their results.

void save_benchmark_plot(
    const std::vector<PlotResult>& results,
    const std::vector<Algorithm>& algorithms,
    const std::string& filename = "benchmark_results.svg"
) {
    if (results.empty()) {
        throw std::invalid_argument(
            "No benchmark results to plot"
        );
    }

    std::vector<std::size_t> sizes;

    for (const auto& r : results) {
        if (r.n > 0) {
            sizes.push_back(r.n);
        }
    }

    std::sort(sizes.begin(), sizes.end());

    sizes.erase(
        std::unique(sizes.begin(), sizes.end()),
        sizes.end()
    );

    if (sizes.empty()) {
        throw std::invalid_argument(
            "No valid array sizes to plot"
        );
    }

    const int width = kPlotWidth;

    const int height =
        70 + static_cast<int>(kPositions.size()) * kPanelHeight;

    std::ofstream out(filename);

    if (!out) {
        throw std::runtime_error(
            "Cannot create SVG file: " + filename
        );
    }

    out << std::fixed << std::setprecision(2);

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << width
        << "\" height=\"" << height
        << "\" viewBox=\"0 0 "
        << width << " " << height << "\">\n";

    out << "<rect width=\"100%\" height=\"100%\" "
        << "fill=\"white\"/>\n";

    out << "<text x=\"" << width / 2
        << "\" y=\"28\" text-anchor=\"middle\" "
        << "font-family=\"sans-serif\" font-size=\"21\" "
        << "font-weight=\"bold\">"
        << "Selection algorithms benchmark (median of "
        << kRepetitions << " runs)</text>\n";

    // Dynamic legend.
    int legend_x = 35;
    int legend_y = 52;

    for (const auto& algorithm : algorithms) {
        if (legend_x > width - 240) {
            legend_x = 35;
            legend_y += 20;
        }

        out << "<line x1=\"" << legend_x
            << "\" y1=\"" << legend_y
            << "\" x2=\"" << legend_x + 22
            << "\" y2=\"" << legend_y
            << "\" stroke=\"" << algorithm.color
            << "\" stroke-width=\"3\"/>\n";

        out << "<text x=\"" << legend_x + 28
            << "\" y=\"" << legend_y + 5
            << "\" font-family=\"sans-serif\" "
            << "font-size=\"12\">"
            << xml_escape(algorithm.name)
            << "</text>\n";

        legend_x +=
            55 + static_cast<int>(algorithm.name.size()) * 7;
    }

    // Five panels: Minimum, 10%, Median, 90%, Maximum.
    for (std::size_t p = 0; p < kPositions.size(); ++p) {
        const int panel_y =
            70 + static_cast<int>(p) * kPanelHeight;

        const int plot_top = panel_y + kPlotTop;

        const int plot_bottom =
            panel_y + kPanelHeight - kPlotBottom;

        const int plot_left = kPlotLeft;
        const int plot_right = width - kPlotRight;

        double max_time = 0.0;

        for (const auto& r : results) {
            if (r.position != kPositions[p].name) {
                continue;
            }

            for (const auto& a : r.algorithms) {
                max_time = std::max(max_time, a.time_ms);
            }
        }

        if (max_time <= 0.0) {
            max_time = 1.0;
        }

        max_time *= 1.10;

        out << "<text x=\"" << plot_left
            << "\" y=\"" << panel_y + 20
            << "\" font-family=\"sans-serif\" "
            << "font-size=\"15\" font-weight=\"bold\">"
            << xml_escape(kPositions[p].name)
            << "</text>\n";

        // Horizontal grid and time labels.
        for (int tick = 0; tick <= 4; ++tick) {
            const double value = max_time * tick / 4.0;

            const int y = plot_bottom -
                (plot_bottom - plot_top) * tick / 4;

            out << "<line x1=\"" << plot_left
                << "\" y1=\"" << y
                << "\" x2=\"" << plot_right
                << "\" y2=\"" << y
                << "\" stroke=\"#e5e7eb\"/>\n";

            out << "<text x=\"" << plot_left - 9
                << "\" y=\"" << y + 4
                << "\" text-anchor=\"end\" "
                << "font-family=\"sans-serif\" "
                << "font-size=\"10\">"
                << value << "</text>\n";
        }

        // Axes.
        out << "<line x1=\"" << plot_left
            << "\" y1=\"" << plot_top
            << "\" x2=\"" << plot_left
            << "\" y2=\"" << plot_bottom
            << "\" stroke=\"#374151\"/>\n";

        out << "<line x1=\"" << plot_left
            << "\" y1=\"" << plot_bottom
            << "\" x2=\"" << plot_right
            << "\" y2=\"" << plot_bottom
            << "\" stroke=\"#374151\"/>\n";

        out << "<text transform=\"translate(24,"
            << (plot_top + plot_bottom) / 2
            << ") rotate(-90)\" text-anchor=\"middle\" "
            << "font-family=\"sans-serif\" font-size=\"12\">"
            << "Time (ms)</text>\n";

        out << "<text x=\""
            << (plot_left + plot_right) / 2
            << "\" y=\"" << plot_bottom + 36
            << "\" text-anchor=\"middle\" "
            << "font-family=\"sans-serif\" font-size=\"11\">"
            << "Array size N (log scale)</text>\n";

        // X-axis labels.
        for (const std::size_t n : sizes) {
            const double x = x_for_n(
                n, sizes.front(), sizes.back(),
                plot_left, plot_right
            );

            out << "<text x=\"" << x
                << "\" y=\"" << plot_bottom + 15
                << "\" text-anchor=\"middle\" "
                << "font-family=\"sans-serif\" "
                << "font-size=\"10\">"
                << n << "</text>\n";
        }

        // Draw every registered algorithm.
        for (std::size_t a = 0; a < algorithms.size(); ++a) {
            std::ostringstream points;
            bool first = true;

            for (const std::size_t n : sizes) {
                const PlotResult* found = nullptr;

                for (const auto& r : results) {
                    if (r.n == n &&
                        r.position == kPositions[p].name) {
                        found = &r;
                        break;
                    }
                }

                if (!found || a >= found->algorithms.size()) {
                    continue;
                }

                const double value =
                    found->algorithms[a].time_ms;

                const double x = x_for_n(
                    n, sizes.front(), sizes.back(),
                    plot_left, plot_right
                );

                const double y = plot_bottom -
                    (plot_bottom - plot_top) *
                    value / max_time;

                if (!first) {
                    points << ' ';
                }

                points << x << ',' << y;
                first = false;

                out << "<circle cx=\"" << x
                    << "\" cy=\"" << y
                    << "\" r=\"3.5\" fill=\""
                    << algorithms[a].color
                    << "\"/>\n";
            }

            out << "<polyline fill=\"none\" stroke=\""
                << algorithms[a].color
                << "\" stroke-width=\"2.2\" points=\""
                << points.str()
                << "\"/>\n";
        }
    }

    out << "</svg>\n";

    if (!out) {
        throw std::runtime_error(
            "Failed to write SVG file: " + filename
        );
    }
}

// ============================================================
// 11. RUN ONE TEST
// ============================================================

void run_test(
    const std::vector<Value>& original,
    const TestPosition& position,
    std::size_t h,
    std::uint64_t sample_seed,
    const std::vector<Algorithm>& algorithms,
    std::vector<PlotResult>& plot_results
) {
    const std::size_t n = original.size();

    const std::size_t k = static_cast<std::size_t>((n - 1) * position.fraction);

    // Sort a separate copy for validation and rank statistics.
    std::vector<Value> sorted = original;

    std::sort(sorted.begin(), sorted.end());

    PlotResult plot_result{
        n,
        position.name,
        {}
    };

    plot_result.algorithms.reserve(algorithms.size());

    bool all_valid = true;

    // The same loop handles every registered algorithm.
    for (const auto& algorithm : algorithms) {
        const auto measurement = measure_median_ms(
            original,
            k,
            [&](std::vector<Value>& a, std::size_t kk) {
                return algorithm.select(
                    a, kk, h, sample_seed
                );
            }
        );

        const bool valid = algorithm.exact
            ? measurement.result == sorted[k]
            : rank_is_acceptable(
                sorted, measurement.result, k, h
            );

        const std::size_t error = rank_error(
            sorted, measurement.result, k
        );

        plot_result.algorithms.push_back({
            algorithm.name,
            algorithm.color,
            measurement.median_ms,
            error,
            valid
        });

        if (!valid) {
            all_valid = false;

            std::cerr
                << "Validation failed: algorithm="
                << algorithm.name
                << ", position=" << position.name
                << ", n=" << n
                << ", k=" << k
                << ", result=" << measurement.result
                << ", expected=" << sorted[k]
                << ", rank_error=" << error
                << '\n';
        }
    }

    print_table_row(
        position.name,
        n,
        k,
        plot_result.algorithms
    );

    plot_results.push_back(std::move(plot_result));

    if (!all_valid) {
        throw std::runtime_error(
            "Algorithm validation failed"
        );
    }
}

// ============================================================
// 12. MAIN
// ============================================================

int main(int argc, char* argv[]) {
    std::vector<std::size_t> sizes;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            const auto parsed = std::stoull(argv[i]);

            if (parsed == 0) {
                std::cerr
                    << "Array sizes must be positive\n";

                return 1;
            }

            sizes.push_back(
                static_cast<std::size_t>(parsed)
            );
        }
    } else {
        sizes = {
            1'000'00,
            10'000'00,
            100'000'00
        };
    }

    // All supported algorithms are registered here.
    const std::vector<Algorithm> algorithms =
        create_algorithms();

    if (algorithms.empty()) {
        std::cerr << "No algorithms registered\n";
        return 1;
    }

    std::vector<PlotResult> plot_results;

    try {
        for (const std::size_t n : sizes) {
            const auto data = generate_data(n, kDataSeed);

            // h proportional to sqrt(n).
            const std::size_t h =
                static_cast<std::size_t>(
                    std::ceil(
                        8.0 * std::sqrt(
                            static_cast<double>(n)
                        )
                    )
                );

            const long double epsilon =
                static_cast<long double>(h) / n;

            const long double raw_m =
                (
                    std::log(2.0L) -
                    std::log(
                        static_cast<long double>(kDelta)
                    )
                ) /
                (2.0L * epsilon * epsilon);

            const long double sample_size =
                std::ceil(raw_m);

            std::cout
                << "\nN=" << n
                << " | h=" << h
                << " | delta=" << kDelta
                << " | calculated sample m=";

            if (sample_size >= static_cast<long double>(n)) {
                std::cout << "fallback (m >= n)\n";
            } else {
                std::cout
                    << static_cast<std::size_t>(sample_size)
                    << '\n';
            }

            print_table_header(algorithms);

            for (const auto& position : kPositions) {
                run_test(
                    data,
                    position,
                    h,
                    kSampleSeed,
                    algorithms,
                    plot_results
                );
            }
        }

        save_benchmark_plot(
            plot_results,
            algorithms
        );

        std::cout
            << "\nPlot saved to benchmark_results.svg\n";
    } catch (const std::exception& e) {
        std::cerr
            << "Benchmark failed: "
            << e.what() << '\n';

        return 1;
    }

    return 0;
}
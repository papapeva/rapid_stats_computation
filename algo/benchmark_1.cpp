#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

 

#include <fstream>
#include <array>
#include <cmath>
#include <sstream>

#include "floyd_rivest.hpp"
#include "random_sampling.hpp"
#include "standard_select.hpp"

using Value = std::int64_t;
using Clock = std::chrono::steady_clock;

template <typename Func>
double benchmark_ms(Func&& func) {
    const auto start = Clock::now();
    func();
    const auto finish = Clock::now();
    return std::chrono::duration<double, std::milli>(finish - start).count();
}

std::vector<Value> generate_data(std::size_t n, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    // Ограниченный диапазон намеренно создаёт повторяющиеся значения.
    std::uniform_int_distribution<Value> dist(0, 1'000'000);

    std::vector<Value> data(n);
    for (auto& x : data) {
        x = dist(rng);
    }
    return data;
}

// Проверяем допустимый ранг с учётом повторяющихся значений.
// sorted — отсортированная исходная последовательность.
// Допуск задаётся вокруг 1-based ранга k+1.
bool rank_is_acceptable(
    const std::vector<Value>& sorted,
    Value value,
    std::size_t k,
    std::size_t h
) {
    const std::size_t n = sorted.size();
    const std::size_t a = (k > h) ? (k - h + 1) : 1;
    const std::size_t b = std::min(n, k + 1 + h);

    const auto lower = std::lower_bound(sorted.begin(), sorted.end(), value);
    const auto upper = std::upper_bound(sorted.begin(), sorted.end(), value);

    if (lower == sorted.end() || *lower != value) {
        return false; // ответ обязан принадлежать исходному массиву
    }

    // Ранги данного значения: [lower_index + 1, upper_index] (1-based).
    const std::size_t first_rank =
        static_cast<std::size_t>(lower - sorted.begin()) + 1;
    const std::size_t last_rank =
        static_cast<std::size_t>(upper - sorted.begin());

    return first_rank <= b && last_rank >= a;
}






struct PlotResult {
    std::size_t n;
    std::string position;
    double floyd_ms;
    double sampling_ms_1;
    double sampling_ms_2;
    double nth_ms;
};

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

    const std::array<std::string, 3> names = {
        "Floyd-Rivest", "Random Sampling", "std::nth_element"
    };

    const std::array<std::string, 3> colors = {
        "#2563eb", "#16a34a", "#dc2626"
    };

    constexpr int width = 1000;
    constexpr int panel_height = 270;
    constexpr int left = 95;
    constexpr int right = 35;
    constexpr int top = 45;
    constexpr int bottom = 55;

    const int height =
        50 + static_cast<int>(positions.size()) * panel_height;

    // SVG — векторный формат, сторонние библиотеки не нужны.
    std::ofstream out(filename);
    if (!out) {
        throw std::runtime_error("Cannot create SVG file: " + filename);
    }

    out << std::fixed << std::setprecision(2);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << width << "\" height=\"" << height
        << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";

    out << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    out << "<text x=\"" << width / 2
        << "\" y=\"30\" text-anchor=\"middle\" "
        << "font-family=\"sans-serif\" font-size=\"21\" "
        << "font-weight=\"bold\">Selection algorithms benchmark</text>\n";

    // Легенда
    int legend_x = 110;
    for (std::size_t a = 0; a < names.size(); ++a) {
        out << "<line x1=\"" << legend_x << "\" y1=\"48\" "
            << "x2=\"" << legend_x + 25
            << "\" y2=\"48\" stroke=\"" << colors[a]
            << "\" stroke-width=\"3\"/>\n";

        out << "<text x=\"" << legend_x + 32
            << "\" y=\"53\" font-family=\"sans-serif\" "
            << "font-size=\"13\">" << names[a] << "</text>\n";

        legend_x += 260;
    }

    std::vector<std::size_t> sizes;
    for (const auto& r : results) {
        if (r.n > 0) {
            sizes.push_back(r.n);
        }
    }

    std::sort(sizes.begin(), sizes.end());
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());

    if (sizes.empty()) {
        throw std::invalid_argument("No valid array sizes to plot");
    }

    const double min_log =
        std::log10(static_cast<double>(sizes.front()));
    const double max_log =
        std::log10(static_cast<double>(sizes.back()));

    for (std::size_t p = 0; p < positions.size(); ++p) {
        const int panel_y = 70 + static_cast<int>(p) * panel_height;
        const int plot_top = panel_y + top;
        const int plot_bottom = panel_y + panel_height - bottom;
        const int plot_left = left;
        const int plot_right = width - right;

        double max_time = 0.0;

        for (const auto& r : results) {
            if (r.position == positions[p]) {
                max_time = std::max({
                    max_time, r.floyd_ms,
                     r.sampling_ms_1, r.sampling_ms_2, r.nth_ms
                });
            }
        }

        if (max_time <= 0.0) {
            continue;
        }

        max_time *= 1.10;

        out << "<text x=\"" << plot_left
            << "\" y=\"" << panel_y + 20
            << "\" font-family=\"sans-serif\" font-size=\"16\" "
            << "font-weight=\"bold\">" << positions[p] << "</text>\n";

        // Оси и горизонтальная сетка
        for (int tick = 0; tick <= 4; ++tick) {
            const double value = max_time * tick / 4.0;
            const int y = plot_bottom -
                (plot_bottom - plot_top) * tick / 4;

            out << "<line x1=\"" << plot_left << "\" y1=\"" << y
                << "\" x2=\"" << plot_right << "\" y2=\"" << y
                << "\" stroke=\"#e5e7eb\"/>\n";

            out << "<text x=\"" << plot_left - 10
                << "\" y=\"" << y + 4
                << "\" text-anchor=\"end\" font-family=\"sans-serif\" "
                << "font-size=\"11\">" << value << "</text>\n";
        }

        out << "<line x1=\"" << plot_left << "\" y1=\"" << plot_top
            << "\" x2=\"" << plot_left << "\" y2=\"" << plot_bottom
            << "\" stroke=\"#374151\"/>\n";

        out << "<line x1=\"" << plot_left << "\" y1=\"" << plot_bottom
            << "\" x2=\"" << plot_right << "\" y2=\"" << plot_bottom
            << "\" stroke=\"#374151\"/>\n";

        out << "<text transform=\"translate(23,"
            << (plot_top + plot_bottom) / 2
            << ") rotate(-90)\" text-anchor=\"middle\" "
            << "font-family=\"sans-serif\" font-size=\"12\">"
            << "Time (ms)</text>\n";

        out << "<text x=\"" << (plot_left + plot_right) / 2
            << "\" y=\"" << plot_bottom + 40
            << "\" text-anchor=\"middle\" "
            << "font-family=\"sans-serif\" font-size=\"12\">"
            << "Array size N</text>\n";

        // Подписи размеров массива по оси X
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            const double x = sizes.size() == 1
                ? (plot_left + plot_right) / 2.0
                : plot_left + (plot_right - plot_left) *
                    (std::log10(static_cast<double>(sizes[i])) - min_log) /
                    std::max(1e-12, max_log - min_log);

            out << "<text x=\"" << x << "\" y=\"" << plot_bottom + 17
                << "\" text-anchor=\"middle\" "
                << "font-family=\"sans-serif\" font-size=\"10\">"
                << sizes[i] << "</text>\n";
        }

        // Три линии: один алгоритм — один цвет.
        for (int algorithm = 0; algorithm < 3; ++algorithm) {
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

                if (!found) {
                    continue;
                }

                const double x = sizes.size() == 1
                    ? (plot_left + plot_right) / 2.0
                    : plot_left + (plot_right - plot_left) *
                        (std::log10(static_cast<double>(n)) - min_log) /
                        std::max(1e-12, max_log - min_log);

                const double value = algorithm == 0
                    ? found->floyd_ms
                    : algorithm == 1
                        ? found->sampling_ms
                        : found->nth_ms;

                const double y = plot_bottom -
                    (plot_bottom - plot_top) * value / max_time;

                if (!first) {
                    points << " ";
                }

                points << x << "," << y;
                first = false;

                out << "<circle cx=\"" << x << "\" cy=\"" << y
                    << "\" r=\"3.5\" fill=\"" << colors[algorithm]
                    << "\"/>\n";
            }

            out << "<polyline fill=\"none\" stroke=\""
                << colors[algorithm]
                << "\" stroke-width=\"2.5\" points=\""
                << points.str() << "\"/>\n";
        }
    }

    out << "</svg>\n";
    out.close();

    if (!out) {
        throw std::runtime_error("Failed to write SVG file: " + filename);
    }
}


std::vector<PlotResult> plot_results;





void run_test(
    const std::vector<Value>& original,
    const std::string& label,
    double k_fraction,
    std::size_t h,
    double delta,
    std::uint64_t seed
) {
    const std::size_t n = original.size();
    const std::size_t k =
        static_cast<std::size_t>((n - 1) * k_fraction);

    // Подготовка эталона и копий вне измеряемых участков.
    std::vector<Value> sorted = original;
    std::sort(sorted.begin(), sorted.end());

    auto a_fr = original;
    auto a_rs = original;
    auto a_std = original;

    Value result_fr{};
    Value result_rs{};
    Value result_std{};

    const double fr_ms = benchmark_ms([&] {
        result_fr = floyd_rivest_select(a_fr, k);
    });

    const double rs_ms_1 = benchmark_ms([&] {
        result_rs = random_sampling_select(
            a_rs, k, h, delta, seed, floyd_rivest_select<Value>);
    });
    
    const double rs_ms_2 = benchmark_ms([&] {
        result_rs = random_sampling_select(
            a_rs, k, h, delta, seed, standard_select<Value>);
    });

    const double std_ms = benchmark_ms([&] {
        result_std = standard_select(a_std, k);
    });

    const bool fr_ok = (result_fr == sorted[k]);
    const bool std_ok = (result_std == sorted[k]);
    const bool rs_ok = rank_is_acceptable(sorted, result_rs, k, h);

    if (!fr_ok || !std_ok || !rs_ok) {
        std::cerr << "Validation failed for " << label
                  << ", n=" << n << ", k=" << k << '\n'
                  << "Floyd-Rivest correct: " << fr_ok << '\n'
                  << "Random Sampling acceptable: " << rs_ok << '\n'
                  << "std::nth_element correct: " << std_ok << '\n';
        throw std::runtime_error("Algorithm validation failed");
    }

    std::cout
        << std::left << std::setw(10) << label
        << std::right << std::setw(12) << n
        << std::setw(12) << k
        << std::setw(12) << fr_ms
        << std::setw(16) << rs_ms_1
        << std::setw(16) << rs_ms_2
        << std::setw(16) << std_ms
        << std::setw(12) << (rs_ok ? "OK" : "FAIL")
        << '\n';
    plot_results.push_back({
        n,
        label,
        fr_ms,
        rs_ms_1, rs_ms_2,
        std_ms
    });
}








int main(int argc, char* argv[]) {
    std::vector<std::size_t> sizes;
    
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            const auto n = std::stoull(argv[i]);
            if (n == 0) {
                std::cerr << "Array sizes must be positive\n";
                return 1;
            }
            sizes.push_back(static_cast<std::size_t>(n));
        }
    } else {
        sizes = {1000000, 10000000, 100000000, 200000000};
    }

    // Настройки Random Sampling:
    constexpr std::size_t h = 100;
    constexpr double delta = 0.1;
    constexpr std::uint64_t data_seed = 42;
    constexpr std::uint64_t sample_seed = 12345;

    try {
        for (const std::size_t n : sizes) {
            const auto data = generate_data(n, data_seed);

            std::cout << "\nArray size: " << n
                      << " | h=" << h
                      << " | delta=" << delta << '\n';

            std::cout
                << std::left << std::setw(10) << "Position"
                << std::right << std::setw(12) << "N"
                << std::setw(12) << "K (0-based)"
                << std::setw(12) << "FR ms"
                << std::setw(16) << "Sampling ms"
                << std::setw(16) << "nth_element"
                << std::setw(12) << "RS check"
                << '\n';

            run_test(data, "Minimum", 0.0, h, delta, sample_seed);
            run_test(data, "10%", 0.1, h, delta, sample_seed);
            run_test(data, "Median", 0.5, h, delta, sample_seed);
            run_test(data, "90%", 0.9, h, delta, sample_seed);
            run_test(data, "Maximum", 1.0, h, delta, sample_seed);
        }
        save_benchmark_plot(plot_results);
        std::cout << "\nPlot saved to benchmark_results.svg\n";
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}

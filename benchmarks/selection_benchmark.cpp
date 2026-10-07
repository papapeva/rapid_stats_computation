#include "rapid_stats/order_statistic.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace {

struct CountingLess {
    std::size_t& comparisons;

    [[nodiscard]] bool operator()(int left, int right) const {
        ++comparisons;
        return left < right;
    }
};

// Memory barrier so a pure, header-visible selection is not hoisted out of the timing loop.
template <typename T> void do_not_optimize(const T& value) {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    asm volatile("" : : "g"(&value) : "memory");
#pragma GCC diagnostic pop
#else
    volatile const unsigned char* bytes = reinterpret_cast<volatile const unsigned char*>(&value);
    const unsigned char byte = *bytes;
    static_cast<void>(byte);
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

template <typename Fn> [[nodiscard]] double best_milliseconds(int repeats, Fn&& function) {
    auto best = std::chrono::steady_clock::duration::max();
    for (int repeat = 0; repeat < repeats; ++repeat) {
        const auto started = std::chrono::steady_clock::now();
        function();
        const auto elapsed = std::chrono::steady_clock::now() - started;
        if (elapsed < best) {
            best = elapsed;
        }
    }
    return std::chrono::duration<double, std::milli>(best).count();
}

struct Measurement {
    const char* name;
    std::ptrdiff_t rank;
    double milliseconds;
    std::size_t comparisons;
    int value;
};

template <typename Select>
[[nodiscard]] bool measure(const char* name, std::ptrdiff_t rank, int expected,
                           const std::vector<int>& sample, int repeats, Select&& select,
                           Measurement& out) {
    int timed_value = 0;
    const double milliseconds = best_milliseconds(repeats, [&] {
        do_not_optimize(sample);
        timed_value = select(sample, rank, std::less<int>{});
        do_not_optimize(timed_value);
    });
    if (timed_value != expected) {
        std::cerr << "mismatch for " << name << " k=" << rank << ": got " << timed_value
                  << ", expected " << expected << '\n';
        return false;
    }

    std::size_t comparisons = 0;
    const int counted_value = select(sample, rank, CountingLess{comparisons});
    if (counted_value != expected) {
        std::cerr << "mismatch for " << name << " k=" << rank << ": got " << counted_value
                  << ", expected " << expected << '\n';
        return false;
    }

    out = Measurement{name, rank, milliseconds, comparisons, timed_value};
    return true;
}

void print_row(const Measurement& row) {
    std::cout << std::left << std::setw(18) << row.name << std::right << std::setw(12) << row.rank
              << std::setw(12) << std::fixed << std::setprecision(2) << row.milliseconds
              << std::setw(16) << row.comparisons << '\n';
}

[[nodiscard]] bool parse_sample_size(int argc, char** argv, std::size_t& sample_size) {
    if (argc > 2) {
        return false;
    }
    if (argc == 2) {
        const auto parsed = std::strtoull(argv[1], nullptr, 10);
        if (parsed < 2ULL || parsed > 50000000ULL) {
            return false;
        }
        sample_size = static_cast<std::size_t>(parsed);
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    constexpr int repeats = 5;
    constexpr std::uint32_t seed = 42;
    std::size_t sample_size = 1000000;
    if (!parse_sample_size(argc, argv, sample_size)) {
        std::cerr << "usage: rapid_stats_bench [n]\n"
                  << "n is the sample size, from 2 to 50000000 (default 1000000)\n";
        return 2;
    }

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 1000000);
    std::vector<int> sample(sample_size);
    for (int& value : sample) {
        value = dist(rng);
    }

    auto sorted = sample;
    std::sort(sorted.begin(), sorted.end());

    const auto n = static_cast<std::ptrdiff_t>(sample_size);
    std::vector<std::ptrdiff_t> ranks;
    const auto add_rank = [&](std::ptrdiff_t rank) {
        if (rank < 0 || rank >= n) {
            return;
        }
        if (std::find(ranks.begin(), ranks.end(), rank) == ranks.end()) {
            ranks.push_back(rank);
        }
    };
    add_rank(0);
    add_rank(10);
    add_rank(n / 2);
    add_rank(n - 1);

    std::cout << "n=" << sample_size << " seed=" << seed << " repeats=" << repeats << '\n'
              << "milliseconds are the best of " << repeats
              << " runs and include copying the sample\n"
              << "comparisons are counted on a separate run\n";
    std::cout << std::left << std::setw(18) << "algorithm" << std::right << std::setw(12) << "k"
              << std::setw(12) << "ms" << std::setw(16) << "comparisons" << '\n';

    for (const auto rank : ranks) {
        const int expected = sorted[static_cast<std::size_t>(rank)];

        Measurement row{};
        const auto nth_element_select = [](const std::vector<int>& data, std::ptrdiff_t k,
                                           auto comp) {
            auto copy = data;
            const auto nth = copy.begin() + k;
            std::nth_element(copy.begin(), nth, copy.end(), comp);
            return *nth;
        };
        if (!measure("std::nth_element", rank, expected, sample, repeats, nth_element_select,
                     row)) {
            return 1;
        }
        print_row(row);

        for (const auto algorithm : rapid_stats::kAlgorithms) {
            const auto select = [algorithm](const std::vector<int>& data, std::ptrdiff_t k,
                                            auto comp) {
                return rapid_stats::order_statistic(data.begin(), data.end(), k, algorithm, comp);
            };
            if (!measure(rapid_stats::algorithm_name(algorithm), rank, expected, sample, repeats,
                         select, row)) {
                return 1;
            }
            print_row(row);
        }
        std::cout << '\n';
    }
    return 0;
}

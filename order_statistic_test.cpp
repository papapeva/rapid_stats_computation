#include "order_statistic.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

int g_failures = 0;

void expect(bool condition, const char* text) {
    if (!condition) {
        std::cerr << "failed: " << text << '\n';
        ++g_failures;
    }
}

void expect_eq(int actual, int expected) {
    if (actual != expected) {
        std::cerr << "failed: " << actual << " != " << expected << '\n';
        ++g_failures;
    }
}

template <typename Fn> void expect_out_of_range(Fn&& function) {
    try {
        function();
        std::cerr << "failed: expected std::out_of_range\n";
        ++g_failures;
    } catch (const std::out_of_range&) {
    } catch (const std::exception& ex) {
        std::cerr << "failed: unexpected exception: " << ex.what() << '\n';
        ++g_failures;
    }
}

void expect_sample(const std::vector<int>& values, std::ptrdiff_t k, int expected) {
    for (const auto algorithm : rapid_stats::kAlgorithms) {
        expect_eq(rapid_stats::order_statistic(values.begin(), values.end(), k, algorithm),
                  expected);
    }
}

} // namespace

int main() {
    expect_sample({42}, 0, 42);
    expect_sample({1, 0}, 0, 0);
    expect_sample({1, 0}, 1, 1);
    expect_sample({4, 3, 2, 1}, 0, 1);
    expect_sample({4, 3, 2, 1}, 3, 4);
    expect_sample({5, 1, 4, 2, 3}, 2, 3);
    expect_sample({2, 2, 2, 2}, 1, 2);
    expect_sample({-5, 0, -1, 4}, 1, -1);

    std::vector<int> values{3, 1, 4, 1, 5};
    const auto original = values;
    for (const auto algorithm : rapid_stats::kAlgorithms) {
        expect_eq(rapid_stats::order_statistic(values.begin(), values.end(), 2, algorithm), 3);
        expect(values == original, "input was modified");
    }

    const std::vector<int> empty;
    const std::vector<int> triple{1, 2, 3};
    for (const auto algorithm : rapid_stats::kAlgorithms) {
        expect_out_of_range([&] {
            static_cast<void>(
                rapid_stats::order_statistic(empty.begin(), empty.end(), 0, algorithm));
        });
        expect_out_of_range([&] {
            static_cast<void>(
                rapid_stats::order_statistic(triple.begin(), triple.end(), -1, algorithm));
        });
        expect_out_of_range([&] {
            static_cast<void>(
                rapid_stats::order_statistic(triple.begin(), triple.end(), 3, algorithm));
        });
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " failed\n";
        return 1;
    }
    std::cout << "ok\n";
    return 0;
}

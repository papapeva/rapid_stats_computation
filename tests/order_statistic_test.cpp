#include "rapid_stats/order_statistic.hpp"

#include "test_harness.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <list>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using rapid_stats::Algorithm;
using rapid_stats::kAlgorithms;
using rapid_stats::order_statistic;

struct CountingLess {
    std::size_t& comparisons;

    [[nodiscard]] bool operator()(int left, int right) const {
        ++comparisons;
        return left < right;
    }
};

void check_sample(std::initializer_list<int> values, std::ptrdiff_t k, int expected) {
    const std::vector<int> sample(values);
    for (const auto algorithm : kAlgorithms) {
        RS_CHECK_EQ(order_statistic(sample.begin(), sample.end(), k, algorithm), expected);
    }
}

} // namespace

RS_TEST(known_samples) {
    check_sample({42}, 0, 42);
    check_sample({1, 0}, 0, 0);
    check_sample({1, 0}, 1, 1);
    check_sample({1, 2, 3, 4}, 0, 1);
    check_sample({1, 2, 3, 4}, 3, 4);
    check_sample({4, 3, 2, 1}, 0, 1);
    check_sample({4, 3, 2, 1}, 2, 3);
    check_sample({5, 1, 4, 2, 3}, 2, 3);
    check_sample({2, 2, 2, 2}, 0, 2);
    check_sample({2, 2, 2, 2}, 3, 2);
    check_sample({-5, 0, -1, 4}, 0, -5);
    check_sample({-5, 0, -1, 4}, 1, -1);
    check_sample({-5, 0, -1, 4}, 3, 4);
}

RS_TEST(does_not_mutate_input) {
    std::vector<int> values{3, 1, 4, 1, 5};
    const auto original = values;
    for (const auto algorithm : kAlgorithms) {
        RS_CHECK_EQ(order_statistic(values.begin(), values.end(), 2, algorithm), 3);
        RS_CHECK(values == original);
    }
}

RS_TEST(accepts_non_random_access_input) {
    const std::list<int> values{4, 1, 3, 2};
    for (const auto algorithm : kAlgorithms) {
        RS_CHECK_EQ(order_statistic(values.begin(), values.end(), 2, algorithm), 3);
    }
}

RS_TEST(works_for_strings) {
    const std::vector<std::string> words{"pear", "apple", "plum"};
    for (const auto algorithm : kAlgorithms) {
        RS_CHECK_EQ(order_statistic(words.begin(), words.end(), 0, algorithm),
                    std::string("apple"));
        RS_CHECK_EQ(order_statistic(words.begin(), words.end(), 1, algorithm), std::string("pear"));
        RS_CHECK_EQ(order_statistic(words.begin(), words.end(), 2, algorithm), std::string("plum"));
    }
}

RS_TEST(custom_order_on_random_samples) {
    std::mt19937 rng(99);
    std::uniform_int_distribution<int> size_dist(1, 30);
    std::uniform_int_distribution<int> value_dist(-10, 10);
    for (int trial = 0; trial < 20; ++trial) {
        std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
        for (int& value : values) {
            value = value_dist(rng);
        }

        auto sorted = values;
        std::sort(sorted.begin(), sorted.end(), std::greater<int>{});
        for (std::size_t index = 0; index < sorted.size(); ++index) {
            const auto k = static_cast<std::ptrdiff_t>(index);
            for (const auto algorithm : kAlgorithms) {
                RS_CHECK_EQ(order_statistic(values.begin(), values.end(), k, algorithm,
                                            std::greater<int>{}),
                            sorted[index]);
            }
        }
    }
}

RS_TEST(respects_custom_order) {
    const std::vector<int> values{1, 5, 3, 5};
    for (const auto algorithm : kAlgorithms) {
        RS_CHECK_EQ(
            order_statistic(values.begin(), values.end(), 0, algorithm, std::greater<int>{}), 5);
        RS_CHECK_EQ(
            order_statistic(values.begin(), values.end(), 1, algorithm, std::greater<int>{}), 5);
        RS_CHECK_EQ(
            order_statistic(values.begin(), values.end(), 2, algorithm, std::greater<int>{}), 3);
        RS_CHECK_EQ(
            order_statistic(values.begin(), values.end(), 3, algorithm, std::greater<int>{}), 1);
    }
}

RS_TEST(rejects_ranks_outside_the_sample) {
    const std::vector<int> empty;
    const std::vector<int> values{1, 2, 3};
    for (const auto algorithm : kAlgorithms) {
        RS_CHECK_THROWS(std::out_of_range,
                        order_statistic(empty.begin(), empty.end(), 0, algorithm));
        RS_CHECK_THROWS(std::out_of_range,
                        order_statistic(values.begin(), values.end(), -1, algorithm));
        RS_CHECK_THROWS(std::out_of_range,
                        order_statistic(values.begin(), values.end(), 3, algorithm));
    }
}

RS_TEST(rejects_unknown_algorithm) {
    const std::vector<int> values{1, 2, 3};
    const auto bogus = static_cast<Algorithm>(100);
    RS_CHECK_THROWS(std::invalid_argument, order_statistic(values.begin(), values.end(), 0, bogus));
}

RS_TEST(all_ranks_on_small_random_samples) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> size_dist(1, 40);
    std::uniform_int_distribution<int> value_dist(-20, 20);

    for (int trial = 0; trial < 50; ++trial) {
        std::vector<int> values(static_cast<std::size_t>(size_dist(rng)));
        for (int& value : values) {
            value = value_dist(rng);
        }

        auto sorted = values;
        std::sort(sorted.begin(), sorted.end());
        for (std::size_t index = 0; index < sorted.size(); ++index) {
            const auto k = static_cast<std::ptrdiff_t>(index);
            for (const auto algorithm : kAlgorithms) {
                RS_CHECK_EQ(order_statistic(values.begin(), values.end(), k, algorithm),
                            sorted[index]);
            }
        }
    }
}

RS_TEST(larger_sample_matches_sorted_oracle) {
    constexpr std::size_t n = 10000;
    std::mt19937 rng(20241007);
    std::uniform_int_distribution<int> dist(-100000, 100000);
    std::vector<int> values(n);
    for (int& value : values) {
        value = dist(rng);
    }

    auto sorted = values;
    std::sort(sorted.begin(), sorted.end());

    const std::ptrdiff_t ranks[] = {
        0,
        1,
        17,
        static_cast<std::ptrdiff_t>(n / 2),
        static_cast<std::ptrdiff_t>(n - 2),
        static_cast<std::ptrdiff_t>(n - 1),
    };
    for (const auto k : ranks) {
        const int expected = sorted[static_cast<std::size_t>(k)];
        for (const auto algorithm : kAlgorithms) {
            RS_CHECK_EQ(order_statistic(values.begin(), values.end(), k, algorithm), expected);
        }
    }
}

RS_TEST(heap_select_scans_the_minimum_with_fewer_comparisons_than_sort) {
    constexpr int n = 64;
    std::vector<int> values(static_cast<std::size_t>(n));
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> dist(-1000, 1000);
    for (int& value : values) {
        value = dist(rng);
    }

    std::size_t heap_comparisons = 0;
    std::size_t sort_comparisons = 0;
    const int by_heap = order_statistic(values.begin(), values.end(), 0, Algorithm::HeapSelect,
                                        CountingLess{heap_comparisons});
    const int by_sort = order_statistic(values.begin(), values.end(), 0, Algorithm::FullSort,
                                        CountingLess{sort_comparisons});
    const int expected = *std::min_element(values.begin(), values.end());

    RS_CHECK_EQ(by_heap, expected);
    RS_CHECK_EQ(by_sort, expected);
    RS_CHECK(heap_comparisons < sort_comparisons);
}

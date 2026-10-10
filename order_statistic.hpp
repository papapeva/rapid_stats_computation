#pragma once

// k-я порядковая статистика на больших выборках.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace rapid_stats {

// Add a new enumerator here and in kAlgorithms, then handle it in order_statistic.
enum class Algorithm : unsigned char {
    FullSort,    // Baseline, O(n log n).
    Quickselect, // Expected O(n), worst case O(n^2).
};

inline constexpr Algorithm kAlgorithms[] = {
    Algorithm::FullSort,
    Algorithm::Quickselect,
};

namespace detail {

template <typename RandomIt, typename Compare>
void order_three(RandomIt low, RandomIt mid, RandomIt high, Compare& comp) {
    if (comp(*mid, *low)) {
        std::iter_swap(mid, low);
    }
    if (comp(*high, *mid)) {
        std::iter_swap(high, mid);
    }
    if (comp(*mid, *low)) {
        std::iter_swap(mid, low);
    }
}

template <typename RandomIt, typename Compare>
[[nodiscard]] RandomIt partition(RandomIt first, RandomIt last, RandomIt pivot_it, Compare& comp) {
    const auto pivot = *pivot_it;
    std::iter_swap(pivot_it, last - 1);

    RandomIt store = first;
    for (RandomIt it = first; it != last - 1; ++it) {
        if (comp(*it, pivot)) {
            std::iter_swap(store, it);
            ++store;
        }
    }
    std::iter_swap(store, last - 1);
    return store;
}

// Places the k-th element at nth. first <= nth < last.
template <typename RandomIt, typename Compare>
void quickselect(RandomIt first, RandomIt nth, RandomIt last, Compare& comp) {
    assert(first <= nth && nth < last);

    while (last - first > 1) {
        if (last - first == 2) {
            if (comp(*(first + 1), *first)) {
                std::iter_swap(first, first + 1);
            }
            return;
        }

        const auto mid = first + (last - first) / 2;
        order_three(first, mid, last - 1, comp);
        const auto pivot = partition(first, last, mid, comp);
        if (nth == pivot) {
            return;
        }
        if (nth < pivot) {
            last = pivot;
        } else {
            first = pivot + 1;
        }
    }
}

} // namespace detail

// Returns the element that would stand at zero-based rank k.
// k == 0 is the minimum for the default std::less. The input range is not modified.
template <typename InputIt, typename Compare = std::less<>>
[[nodiscard]] auto order_statistic(InputIt first, InputIt last, std::ptrdiff_t k,
                                   Algorithm algorithm = Algorithm::Quickselect,
                                   Compare comp = {}) ->
    typename std::iterator_traits<InputIt>::value_type {
    using value_type = typename std::iterator_traits<InputIt>::value_type;
    std::vector<value_type> sample(first, last);
    if (k < 0 || static_cast<std::size_t>(k) >= sample.size()) {
        throw std::out_of_range("order statistic rank is outside the sample");
    }

    const auto nth = sample.begin() + k;
    switch (algorithm) {
    case Algorithm::FullSort:
        std::sort(sample.begin(), sample.end(), comp);
        break;
    case Algorithm::Quickselect:
        detail::quickselect(sample.begin(), nth, sample.end(), comp);
        break;
    default:
        throw std::invalid_argument("unknown order-statistic algorithm");
    }
    return *nth;
}

} // namespace rapid_stats

#pragma once

#include "rapid_stats/detail/requirements.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>

namespace rapid_stats::detail {

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

// Lomuto partition. Returns the final position of the pivot value.
template <typename RandomIt, typename Compare>
[[nodiscard]] RandomIt lomuto_partition(RandomIt first, RandomIt last, RandomIt pivot_it,
                                        Compare& comp) {
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

// Quickselect with Lomuto partition and a median-of-three pivot.
// Expected O(n) comparisons, quadratic on adversarial pivots.
// Worst-case linear selection (median of medians / introselect) is the natural next step.
//
// Invariant: the answer lies in [first, last) and first <= nth < last.
template <typename RandomIt, typename Compare>
void quickselect(RandomIt first, RandomIt nth, RandomIt last, Compare& comp) {
    static_assert(is_random_access<RandomIt>(), "RandomIt must be a random-access iterator");
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
        const auto pivot = lomuto_partition(first, last, mid, comp);
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

} // namespace rapid_stats::detail

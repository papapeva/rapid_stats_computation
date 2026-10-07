#pragma once

#include "rapid_stats/detail/requirements.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <vector>

namespace rapid_stats::detail {

// Places the order statistic at nth.
// A max-heap keeps the rank + 1 best candidates seen so far; its root is the answer.
// Time is O(n log k) with k = nth - first, so this wins when k is much smaller than n.
template <typename RandomIt, typename Compare>
void heap_select(RandomIt first, RandomIt nth, RandomIt last, Compare& comp) {
    static_assert(is_random_access<RandomIt>(), "RandomIt must be a random-access iterator");
    assert(first <= nth && nth < last);

    using value_type = typename std::iterator_traits<RandomIt>::value_type;
    const auto rank = nth - first;

    std::vector<value_type> heap;
    heap.reserve(static_cast<std::size_t>(rank) + 1);

    for (auto it = first; it != last; ++it) {
        if (static_cast<std::ptrdiff_t>(heap.size()) <= rank) {
            heap.push_back(*it);
            std::push_heap(heap.begin(), heap.end(), comp);
            continue;
        }
        if (comp(*it, heap.front())) {
            std::pop_heap(heap.begin(), heap.end(), comp);
            heap.back() = *it;
            std::push_heap(heap.begin(), heap.end(), comp);
        }
    }

    assert(!heap.empty());
    *nth = heap.front();
}

} // namespace rapid_stats::detail

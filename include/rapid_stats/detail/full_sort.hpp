#pragma once

#include "rapid_stats/detail/requirements.hpp"

#include <algorithm>

namespace rapid_stats::detail {

// Sorts [first, last). Every position, including nth, then holds its order statistic.
template <typename RandomIt, typename Compare>
void full_sort(RandomIt first, RandomIt /*nth*/, RandomIt last, Compare& comp) {
    static_assert(is_random_access<RandomIt>(), "RandomIt must be a random-access iterator");
    std::sort(first, last, comp);
}

} // namespace rapid_stats::detail

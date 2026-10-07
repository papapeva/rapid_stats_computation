#pragma once

// k-я порядковая статистика на больших выборках.

#include "rapid_stats/detail/full_sort.hpp"
#include "rapid_stats/detail/heap_select.hpp"
#include "rapid_stats/detail/quickselect.hpp"

#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace rapid_stats {

// Selection methods compared by the project.
// Add a new enumerator in this list, in kAlgorithms, in algorithm_name, and in order_statistic.
enum class Algorithm : unsigned char {
    // Baseline. O(n log n) after the sample is copied.
    FullSort,
    // Expected linear time. Worst case is quadratic; pivots are median-of-three.
    Quickselect,
    // O(n log k). Prefer it when the requested rank is close to the start of the order.
    HeapSelect,
};

// Algorithms exercised by tests and the benchmark. Keep this in sync with the enum.
inline constexpr Algorithm kAlgorithms[] = {
    Algorithm::FullSort,
    Algorithm::Quickselect,
    Algorithm::HeapSelect,
};

[[nodiscard]] inline const char* algorithm_name(Algorithm algorithm) {
    switch (algorithm) {
    case Algorithm::FullSort:
        return "full_sort";
    case Algorithm::Quickselect:
        return "quickselect";
    case Algorithm::HeapSelect:
        return "heap_select";
    default:
        return "unknown";
    }
}

// Returns the element that would stand at zero-based rank k if [first, last) were ordered by comp.
// k == 0 is the first element in that order (the minimum, for the default std::less).
// The input range is not modified: the algorithm runs on a copy, so any input iterator is accepted.
// Compare is taken by value, as in the standard algorithms. A stateful comparator that must report
// results back to the caller should hold a reference or a pointer to that state.
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
        detail::full_sort(sample.begin(), nth, sample.end(), comp);
        break;
    case Algorithm::Quickselect:
        detail::quickselect(sample.begin(), nth, sample.end(), comp);
        break;
    case Algorithm::HeapSelect:
        detail::heap_select(sample.begin(), nth, sample.end(), comp);
        break;
    default:
        throw std::invalid_argument("unknown order-statistic algorithm");
    }
    return *nth;
}

} // namespace rapid_stats

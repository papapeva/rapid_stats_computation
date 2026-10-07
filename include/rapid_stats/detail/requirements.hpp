#pragma once

#include <iterator>
#include <type_traits>

namespace rapid_stats::detail {

template <typename Iterator> [[nodiscard]] constexpr bool is_random_access() {
    return std::is_convertible_v<typename std::iterator_traits<Iterator>::iterator_category,
                                 std::random_access_iterator_tag>;
}

} // namespace rapid_stats::detail

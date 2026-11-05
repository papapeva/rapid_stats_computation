#pragma once

#include "../order_statistic_algorithm.hpp"

#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace rapid_stats::detail {

// Реализации работают с изменяемым std::vector чисел, без копии контейнера.
template <typename T> struct IsNumericVector : std::false_type {};
template <typename T, typename Allocator>
struct IsNumericVector<std::vector<T, Allocator>>
    : std::bool_constant<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>> {};

template <typename Container>
void validate_ranks(const Container& data, const std::vector<std::size_t>& ranks) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: data — исходный массив; ranks — нулевые ранги в порядке запроса.
     * Возвращает: ничего; неверные ранги или неконечные числа вызывают исключение до перестановок.
     */
    static_assert(IsNumericVector<Container>::value, "a numeric std::vector is required");
    for (const auto rank : ranks) {
        if (rank >= data.size()) {
            throw std::out_of_range("rank must be in [0, size)");
        }
    }
    if (ranks.empty()) {
        return;
    }
    if constexpr (std::is_floating_point_v<typename Container::value_type>) {
        /*
     * Автор: Codex (OpenAI).
     * Принимает: ветвь для выбранного числового типа.
     * Возвращает: ничего; это блок if constexpr.
     */
        for (const auto value : data) {
            if (!std::isfinite(value)) {
                throw std::invalid_argument("finite values required");
            }
        }
    }
}

template <typename Container>
std::vector<typename Container::iterator> rank_iterators(Container& data,
                                                         const std::vector<std::size_t>& ranks) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: data с установленными статистиками и ranks с нулевыми рангами.
     * Возвращает: итераторы в порядке запроса, включая повторяющиеся ранги.
     */
    std::vector<typename Container::iterator> result;
    result.reserve(ranks.size());
    for (const auto rank : ranks) {
        result.push_back(data.begin() + static_cast<typename Container::difference_type>(rank));
    }
    return result;
}
} // namespace rapid_stats::detail

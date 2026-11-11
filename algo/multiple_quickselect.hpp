#pragma once

#include "order_statistic_algorithm.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <optional>
#include <utility>
#include <vector>

namespace rapid_stats {

// Точный поиск нескольких порядковых статистик за один запуск.
// Массив может переставляться. Для поиска используется случайный опорный элемент.
template <typename Container>
class MultipleQuickselectAlgorithm : public OrderStatisticAlgorithm<Container> {
public:
    using iterator = typename OrderStatisticAlgorithm<Container>::iterator;
    using value_type = typename std::iterator_traits<iterator>::value_type;
    using difference_type = typename std::iterator_traits<iterator>::difference_type;

    std::vector<value_type> sort(Container& data,
                                 const std::vector<std::size_t>& ranks,
                                 std::uint64_t seed) {
        /*
         * Автор: Valeriya Sokolova
         * Принимает: data — изменяемая выборка, ranks — ранги от 1 до n,
         * seed — начальное состояние генератора случайных чисел.
         * Возвращает: значения порядковых статистик в порядке ranks.
         */
        const auto distance = std::distance(std::begin(data), std::end(data));

        if (distance < 0) {
            throw std::invalid_argument("Invalid iterator range");
        }

        const std::size_t n = static_cast<std::size_t>(distance);

        if (n == 0) {
            throw std::invalid_argument("Array must not be empty");
        }

        if (ranks.empty()) {
            throw std::invalid_argument("Ranks must not be empty");
        }

        for (const std::size_t rank : ranks) {
            if (rank < 1 || rank > n) {
                throw std::out_of_range("Rank must be in [1, n]");
            }
        }

        if constexpr (std::is_same_v<std::remove_cv_t<value_type>, double>) {
            for (auto it = std::begin(data); it != std::end(data); ++it) {
                if (!std::isfinite(*it)) {
                    throw std::invalid_argument(
                        "Array must contain only finite double values");
                }
            }
        }

        // Список запросов: ранг и найденное значение.
        struct Query {
            std::size_t rank;
            std::optional<value_type> value;
        };

        std::vector<Query> queries;
        queries.reserve(ranks.size());

        for (const std::size_t rank : ranks) {
            queries.push_back({rank, std::nullopt});
        }

        // Сортируем запросы по возрастанию ранга.
        std::sort(queries.begin(), queries.end(),
                  [](const Query& lhs, const Query& rhs) {
                      return lhs.rank < rhs.rank;
                  });

        // Генератор создаётся один раз на весь запуск.
        std::mt19937_64 rng(seed);

        // Начальная задача помещается в явный стек.
        struct Frame {
            std::size_t lo;
            std::size_t hi;
            std::size_t begin;
            std::size_t end;
        };

        std::vector<Frame> stack;
        stack.push_back({0, n - 1, 0, queries.size()});

        const iterator first = std::begin(data);

        while (!stack.empty()) {
            const Frame frame = stack.back();
            stack.pop_back();

            const std::size_t lo = frame.lo;
            const std::size_t hi = frame.hi;
            const std::size_t b = frame.begin;
            const std::size_t e = frame.end;

            if (b == e) {
                continue;
            }

            // Если остался один элемент, он соответствует всем запросам.
            if (lo == hi) {
                for (std::size_t j = b; j < e; ++j) {
                    queries[j].value = *(first + static_cast<difference_type>(lo));
                }

                continue;
            }

            // Случайно выбираем опорный элемент.
            const std::size_t pivot_index = random_index(rng, lo, hi);
            const value_type pivot =
                *(first + static_cast<difference_type>(pivot_index));

            // Разбиваем текущую часть на три группы.
            const auto [lt, gt] = partition3(first, lo, hi, pivot);

            // Определяем запросы, попавшие в группу равных элементов.
            const auto u_it = std::lower_bound(
                queries.begin() + static_cast<std::ptrdiff_t>(b),
                queries.begin() + static_cast<std::ptrdiff_t>(e),
                lt + 1,
                [](const Query& query, std::size_t rank) {
                    return query.rank < rank;
                });

            const auto v_it = std::upper_bound(
                queries.begin() + static_cast<std::ptrdiff_t>(b),
                queries.begin() + static_cast<std::ptrdiff_t>(e),
                gt + 1,
                [](std::size_t rank, const Query& query) {
                    return rank < query.rank;
                });

            const std::size_t u =
                static_cast<std::size_t>(u_it - queries.begin());

            const std::size_t v =
                static_cast<std::size_t>(v_it - queries.begin());

            // Все ранги из равной части уже найдены.
            for (std::size_t j = u; j < v; ++j) {
                queries[j].value = pivot;
            }

            // Правую часть помещаем первой, чтобы левой части
            // соответствовал порядок обработки рекурсивного алгоритма.
            if (v < e) {
                stack.push_back({gt + 1, hi, v, e});
            }

            if (b < u) {
                stack.push_back({lo, lt - 1, b, u});
            }
        }

        // Восстанавливаем исходный порядок запрошенных рангов.
        std::vector<value_type> result;
        result.reserve(ranks.size());

        for (const std::size_t rank : ranks) {
            const auto it = std::lower_bound(
                queries.begin(), queries.end(), rank,
                [](const Query& query, std::size_t value) {
                    return query.rank < value;
                });

            if (it == queries.end() || it->rank != rank || !it->value) {
                throw std::logic_error("Requested rank was not found");
            }

            result.push_back(*it->value);
        }

        return result;
    }

private:
    // Равномерный случайный выбор индекса из [lo, hi].
    static std::size_t random_index(std::mt19937_64& rng,
                                    std::size_t lo,
                                    std::size_t hi) {
        std::uniform_int_distribution<std::size_t> distribution(lo, hi);
        return distribution(rng);
    }

    // Трёхчастное разбиение вокруг опорного значения.
    // [lo, lt) — меньше pivot.
    // [lt, gt] — равно pivot.
    // (gt, hi] — больше pivot.
    static std::pair<std::size_t, std::size_t> partition3(
        iterator first,
        std::size_t lo,
        std::size_t hi,
        const value_type& pivot) {

        /*
         * Автор: Valeriya Sokolova
         * Принимает: first — начало массива, lo и hi — границы участка,
         * pivot — опорное значение.
         * Возвращает: границы группы элементов, равных опоре.
         */

        // Используем знаковый тип для безопасного уменьшения gt.
        difference_type lt = static_cast<difference_type>(lo);
        difference_type i = static_cast<difference_type>(lo);
        difference_type gt = static_cast<difference_type>(hi);

        while (i <= gt) {
            if (*(first + i) < pivot) {
                std::iter_swap(first + lt, first + i);
                ++lt;
                ++i;
            } else if (pivot < *(first + i)) {
                std::iter_swap(first + i, first + gt);
                --gt;
            } else {
                ++i;
            }
        }

        return {
            static_cast<std::size_t>(lt),
            static_cast<std::size_t>(gt)
        };
    }
};

} // namespace rapid_stats


// Файл реализации алгоритма точного нахождения k-статистики методом Floyd-Rivest
// Класс class FloydRivestSelect наследуется от OrderStatisticAlgorithm
// На вход должен подаваться массив, содержащий только один ранк, может потом алгоритм будет менятся в изменением логини проекта
// пример использования:

// std::vector<int> data = {9, 2, 7, 1, 5, 3, 8};
// std::vector<std::size_t> ranks = {2};
// rapid_stats::FloydRivestAlgorithm<std::vector<int>> algorithm(data);
// auto result = algorithm.sort(ranks);
// возвращается объект типа std::vector<iterator> - найденный статистики
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "order_statistic_algorithm.hpp"

namespace rapid_stats {

namespace detail {

// Границы диапазона элементов,
// эквивалентных опорному элементу.
struct Partition {
    std::ptrdiff_t equal_begin;
    std::ptrdiff_t equal_end;
};


// Выбор элемента ранга k в диапазоне [left, right].
//
// После разбиения возвращает границы группы элементов,
// эквивалентных выбранному элементу.
//
// Используем std::ptrdiff_t для индексов,
// чтобы избежать проблем с беззнаковым переполнением.
template <typename T, typename Compare>
Partition floyd_rivest_select(
    std::vector<T>& data,
    std::ptrdiff_t left,
    std::ptrdiff_t right,
    std::ptrdiff_t k,
    Compare comp) {

    while (right > left) {

        const std::ptrdiff_t n = right - left + 1;

        // Для больших диапазонов используем
        // рекурсивный выборочный поиск.
        if (n > 600) {

            const long double i =
                static_cast<long double>(k - left + 1);

            const long double size =
                static_cast<long double>(n);

            const long double z = std::log(size);

            const long double s =
                0.5L * std::exp(2.0L * z / 3.0L);

            long double sd =
                0.5L * std::sqrt(
                    z * s * (size - s) / size
                );

            if (i < size / 2.0L) {
                sd = -sd;
            }

            // Левая граница выборки:
            // left <= sample_left <= k.
            const std::ptrdiff_t sample_left =
                static_cast<std::ptrdiff_t>(
                    std::clamp(
                        std::floor(
                            static_cast<long double>(k)
                            - i * s / size + sd
                        ),
                        static_cast<long double>(left),
                        static_cast<long double>(k)
                    )
                );

            // Правая граница выборки:
            // k <= sample_right <= right.
            const std::ptrdiff_t sample_right =
                static_cast<std::ptrdiff_t>(
                    std::clamp(
                        std::floor(
                            static_cast<long double>(k)
                            + (size - i) * s / size + sd
                        ),
                        static_cast<long double>(k),
                        static_cast<long double>(right)
                    )
                );

            floyd_rivest_select(
                data,
                sample_left,
                sample_right,
                k,
                comp
            );
        }

        const T pivot = data[k];

        std::ptrdiff_t lt = left;
        std::ptrdiff_t i = left;
        std::ptrdiff_t gt = right;

        // Трёхстороннее разбиение:
        //
        // [left, lt)  — меньше pivot;
        // [lt, gt]    — эквивалентны pivot;
        // (gt, right] — больше pivot.
        //
        while (i <= gt) {

            if (comp(data[i], pivot)) {

                std::swap(data[lt], data[i]);

                ++lt;
                ++i;

            } else if (comp(pivot, data[i])) {
                std::swap(data[i], data[gt]);

                --gt;
            } else {
                ++i;
            }
        }

        // Определяем, в какой части находится
        // искомый ранг.
        if (k < lt) {

            right = lt - 1;

        } else if (k > gt) {

            left = gt + 1;

        } else {

            return {lt, gt};
        }
    }

    return {left, right};
}


// Совместный поиск нескольких уникальных рангов.
// ranks должен быть отсортирован по возрастанию.
//
// Сейчас публичный класс поддерживает только один ранг,
// но функция оставлена для возможного расширения.
template <typename T, typename Compare>
void select_multiple(
    std::vector<T>& data,
    const std::vector<std::size_t>& ranks,
    std::size_t rank_begin,
    std::size_t rank_end,
    std::ptrdiff_t left,
    std::ptrdiff_t right,
    Compare comp) {

    if (rank_begin >= rank_end || left > right) {
        return;
    }

    const std::size_t middle =
        rank_begin + (rank_end - rank_begin) / 2;

    const std::ptrdiff_t k =
        static_cast<std::ptrdiff_t>(ranks[middle]);

    const Partition partition =
        floyd_rivest_select(
            data,
            left,
            right,
            k,
            comp
        );

    const auto first_right = std::lower_bound(
        ranks.begin() + rank_begin,
        ranks.begin() + rank_end,
        static_cast<std::size_t>(partition.equal_begin)
    );

    const auto first_after_equal = std::upper_bound(
        ranks.begin() + rank_begin,
        ranks.begin() + rank_end,
        static_cast<std::size_t>(partition.equal_end)
    );

    const std::size_t left_rank_end =
        static_cast<std::size_t>(
            first_right - ranks.begin()
        );

    const std::size_t right_rank_begin =
        static_cast<std::size_t>(
            first_after_equal - ranks.begin()
        );

    // Обрабатываем левую часть.
    if (left < partition.equal_begin &&
        rank_begin < left_rank_end) {

        select_multiple(
            data,
            ranks,
            rank_begin,
            left_rank_end,
            left,
            partition.equal_begin - 1,
            comp
        );
    }

    // Обрабатываем правую часть.
    if (partition.equal_end < right &&
        right_rank_begin < rank_end) {

        select_multiple(
            data,
            ranks,
            right_rank_begin,
            rank_end,
            partition.equal_end + 1,
            right,
            comp
        );
    }
}

} // namespace detail


// Класс алгоритма Floyd–Rivest.
template <typename Container>
class FloydRivestSelect
    : public OrderStatisticAlgorithm<Container> {

public:

    using Base = OrderStatisticAlgorithm<Container>;
    using iterator = typename Base::iterator;

    explicit FloydRivestSelect(Container& data)
        : Base(data) {}


    std::vector<iterator> sort(
        const std::vector<std::size_t>& ranks
    ) override {

        // Алгоритм поддерживает поиск только одного ранга.
        if (ranks.size() > 1) {
            throw std::invalid_argument(
                "Floyd-Rivest algorithm accepts only one rank"
            );
        }

        const auto distance =
            std::distance(
                this->begin_,
                this->end_
            );

        if (distance < 0) {
            throw std::logic_error(
                "Invalid iterator range"
            );
        }

        const std::size_t n =
            static_cast<std::size_t>(distance);

        // Проверяем корректность рангов.
        for (const std::size_t rank : ranks) {

            if (rank >= n) {
                throw std::out_of_range(
                    "Rank is out of range"
                );
            }
        }

        if (ranks.empty()) {
            return {};
        }

        // Проверяем возможность безопасного
        // преобразования индексов в ptrdiff_t.
        if (n > static_cast<std::size_t>(
                std::numeric_limits<std::ptrdiff_t>::max()
            )) {

            throw std::length_error(
                "Container is too large"
            );
        }

        // Сохраняем итераторы исходного контейнера,
        // чтобы не изменять порядок его элементов.
        std::vector<iterator> elements;

        elements.reserve(n);

        for (
            auto it = this->begin_;
            it != this->end_;
            ++it
        ) {
            elements.push_back(it);
        }

        // Компаратор сравнивает значения,
        // на которые указывают итераторы.
        const auto comp = [](
            const iterator& lhs,
            const iterator& rhs
        ) {
            return *lhs < *rhs;
        };

        // Выполняем поиск k-статистики.
        detail::floyd_rivest_select(
            elements,
            0,
            static_cast<std::ptrdiff_t>(n - 1),
            static_cast<std::ptrdiff_t>(ranks[0]),
            comp
        );

        // Возвращаем итератор на найденный элемент
        // в исходном контейнере.
        return {elements[ranks[0]]};
    }
};

} // namespace rapid_stats

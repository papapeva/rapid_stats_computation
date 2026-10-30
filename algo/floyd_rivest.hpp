// Файл реализации алгоритма точного нахождения k-статистик методом Floyd-Rivest
// Класс class FloydRivestSelect наследуется от OrderStatisticAlgorithm
// пример использования:

// std::vector<int> data = {9, 2, 7, 1, 5, 3, 8};
// std::vector<std::size_t> ranks = {2, 5};
// rapid_stats::FloydRivestAlgorithm<std::vector<int>> algorithm(data);
// auto result = algorithm.sort(ranks);
// возвращается объект типа std::vector<iterator> - найденный статистики


#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

#include "order_statistic_algorithm.hpp"

namespace rapid_stats {

namespace detail {

// Границы диапазона элементов, эквивалентных опорному.
struct Partition {
    std::size_t equal_begin;
    std::size_t equal_end;
};

// Выбор элемента ранга k в диапазоне [left, right].
// После разбиения возвращает границы группы элементов,
// эквивалентных выбранному элементу.
template <typename T, typename Compare>
Partition floyd_rivest_select(
    std::vector<T>& data,
    std::size_t left,
    std::size_t right,
    std::size_t k,
    Compare comp) {

    while (right > left) {
        const std::size_t n = right - left + 1;

        if (n > 600) {
            const double i = static_cast<double>(k - left + 1);
            const double size = static_cast<double>(n);
            const double z = std::log(size);
            const double s = 0.5 * std::exp(2.0 * z / 3.0);

            double sd =
                0.5 * std::sqrt(z * s * (size - s) / size);

            if (i < size / 2.0) {
                sd = -sd;
            }

            const auto sample_left = static_cast<std::size_t>(
                std::max(
                    static_cast<double>(left),
                    std::floor(
                        static_cast<double>(k)
                        - i * s / size + sd)));

            const auto sample_right = static_cast<std::size_t>(
                std::min(
                    static_cast<double>(right),
                    std::floor(
                        static_cast<double>(k)
                        + (size - i) * s / size + sd)));

            floyd_rivest_select(
                data, sample_left, sample_right, k, comp);
        }

        const T pivot = data[k];

        std::size_t lt = left;
        std::size_t i = left;
        std::size_t gt = right;

        // Трёхстороннее разбиение:
        // [left, lt) — меньше pivot;
        // [lt, gt] — эквивалентны pivot;
        // (gt, right] — больше pivot.
        while (i <= gt) {
            if (comp(data[i], pivot)) {
                std::swap(data[lt], data[i]);
                ++lt;
                ++i;
            } else if (comp(pivot, data[i])) {
                std::swap(data[i], data[gt]);

                if (gt == 0) {
                    break;
                }

                --gt;
            } else {
                ++i;
            }
        }

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
// ranks отсортирован по возрастанию.
template <typename T, typename Compare>
void select_multiple(
    std::vector<T>& data,
    const std::vector<std::size_t>& ranks,
    std::size_t rank_begin,
    std::size_t rank_end,
    std::size_t left,
    std::size_t right,
    Compare comp) {

    if (rank_begin >= rank_end || left >= right) {
        return;
    }

    const std::size_t middle =
        rank_begin + (rank_end - rank_begin) / 2;

    const std::size_t k = ranks[middle];

    const Partition partition =
        floyd_rivest_select(data, left, right, k, comp);

    const auto first_right = std::lower_bound(
        ranks.begin() + rank_begin,
        ranks.begin() + rank_end,
        partition.equal_begin);

    const auto first_after_equal = std::upper_bound(
        ranks.begin() + rank_begin,
        ranks.begin() + rank_end,
        partition.equal_end);

    const std::size_t left_rank_end =
        static_cast<std::size_t>(first_right - ranks.begin());

    const std::size_t right_rank_begin =
        static_cast<std::size_t>(
            first_after_equal - ranks.begin());

    if (left < partition.equal_begin &&
        rank_begin < left_rank_end) {

        select_multiple(
            data,
            ranks,
            rank_begin,
            left_rank_end,
            left,
            partition.equal_begin - 1,
            comp);
    }

    if (partition.equal_end < right &&
        right_rank_begin < rank_end) {

        select_multiple(
            data,
            ranks,
            right_rank_begin,
            rank_end,
            partition.equal_end + 1,
            right,
            comp);
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
        const std::vector<std::size_t>& ranks) override {

        const auto distance =
            std::distance(this->begin_, this->end_);

        if (distance < 0) {
            throw std::logic_error("Invalid iterator range");
        }

        const std::size_t n =
            static_cast<std::size_t>(distance);

        for (const std::size_t rank : ranks) {
            if (rank >= n) {
                throw std::out_of_range("Rank is out of range");
            }
        }

        if (ranks.empty()) {
            return {};
        }

        // Храним итераторы, чтобы не менять исходный контейнер.
        std::vector<iterator> elements;
        elements.reserve(n);

        for (auto it = this->begin_; it != this->end_; ++it) {
            elements.push_back(it);
        }

        // Убираем повторяющиеся ранги для вычислений.
        std::vector<std::size_t> unique_ranks = ranks;

        std::sort(unique_ranks.begin(), unique_ranks.end());

        unique_ranks.erase(
            std::unique(unique_ranks.begin(), unique_ranks.end()),
            unique_ranks.end());

        const auto comp = [](
            const iterator& lhs,
            const iterator& rhs) {
            return *lhs < *rhs;
        };

        detail::select_multiple(
            elements,
            unique_ranks,
            0,
            unique_ranks.size(),
            0,
            n - 1,
            comp);

        // Возвращаем результаты в исходном порядке ranks.
        std::vector<iterator> result;
        result.reserve(ranks.size());

        for (const std::size_t rank : ranks) {
            result.push_back(elements[rank]);
        }

        return result;
    }
};

} // namespace rapid_stats

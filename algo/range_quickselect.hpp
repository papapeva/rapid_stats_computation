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
#include <utility>
#include <vector>

namespace rapid_stats {

template <typename Container>
class RangeQuickselect : public ApproximateOrderStatisticAlgorithm<Container> {
public:
    using Base = ApproximateOrderStatisticAlgorithm<Container>;
    using iterator = typename Base::iterator;
    using value_type = typename std::iterator_traits<iterator>::value_type;

    static_assert(std::is_arithmetic_v<value_type> &&
                      !std::is_same_v<std::remove_cv_t<value_type>, bool>,
                  "RangeQuickselect requires an arithmetic value type");

    static_assert(std::is_base_of_v<std::random_access_iterator_tag,
                                    typename std::iterator_traits<iterator>::iterator_category>,
                  "RangeQuickselect requires random-access iterators");

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: data — выборка, rank — запрошенный ранг (от 1 до n), eps — допустимое отклонение ранга (доля n),
     * delta — вероятность ошибки (не используется в данной реализации), seed — начальное состояние генератора.
     * Возвращает: итератор на допустимую порядковую статистику.
     */
    std::vector<iterator> sort(Container& data, std::size_t rank, double eps,
                               [[maybe_unused]] double delta, std::uint64_t seed) override {
        partitions_ = 0;
        comparisons_ = 0;

        if (data.empty()) {
            throw std::invalid_argument("Array must not be empty");
        }

        const std::size_t n = data.size();

        if (rank == 0 || rank > n) {
            throw std::out_of_range("Rank must be in range [1, n]");
        }

        if (eps < 0.0 || eps > 1.0) {
            throw std::invalid_argument("eps must be in range [0, 1]");
        }

        auto begin = std::begin(data);
        auto end = std::end(data);

        if constexpr (std::is_floating_point_v<value_type>) {
            for (auto it = begin; it != end; ++it) {
                if (!std::isfinite(*it)) {
                    throw std::invalid_argument("Array must contain only finite numbers");
                }
            }
        }

        // Переводим ранг в 0-based и вычисляем допуск h
        const std::size_t target_rank = rank - 1;
        const std::size_t h = static_cast<std::size_t>(std::ceil(eps * n));

        std::mt19937_64 rng(seed);
        value_type result_value = select_value(data, target_rank, n, h, rng);
        
        return { find_value_iterator(data, result_value) };
    }

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: ничего.
     * Возвращает: число разбиений за последний вызов sort().
     */
    [[nodiscard]] std::uint64_t partitions() const noexcept {
        return partitions_;
    }

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: ничего.
     * Возвращает: число сравнений с опорным элементом за последний вызов sort().
     */
    [[nodiscard]] std::uint64_t comparisons() const noexcept {
        return comparisons_;
    }

private:
    struct PartitionResult {
        std::size_t lt;
        std::size_t gt;
    };

    std::uint64_t partitions_ = 0;
    std::uint64_t comparisons_ = 0;

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: data — контейнер, value — искомое значение.
     * Возвращает: итератор на первое вхождение value в data.
     */
    iterator find_value_iterator(Container& data, const value_type& value) {
        auto it = std::begin(data);
        while (it != std::end(data) && *it != value) {
            ++it;
        }
        if (it == std::end(data)) {
            throw std::logic_error("Selected value was not found");
        }
        return it;
    }

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: lo и hi — границы диапазона включительно, rng — генератор.
     * Возвращает: случайный индекс из [lo, hi].
     */
    std::size_t random_index(std::size_t lo, std::size_t hi, std::mt19937_64& rng) {
        std::uniform_int_distribution<std::size_t> dist(lo, hi);
        return dist(rng);
    }

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: data, lo, hi — границы диапазона, pivot — опорное значение.
     * Возвращает: границы блока элементов, равных pivot.
     */
    PartitionResult partition3(Container& data, std::size_t lo, std::size_t hi, const value_type& pivot) {
        auto begin = std::begin(data);
        std::size_t lt = lo;
        std::size_t i = lo;
        std::size_t gt = hi;

        while (i <= gt) {
            ++comparisons_;
            if (*(begin + i) < pivot) {
                std::iter_swap(begin + lt, begin + i);
                ++lt;
                ++i;
            } else {
                ++comparisons_;
                if (*(begin + i) > pivot) {
                    std::iter_swap(begin + i, begin + gt);
                    --gt;
                } else {
                    ++i;
                }
            }
        }
        return {lt, gt};
    }

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: data, target_rank, n, h, rng — параметры и генератор.
     * Возвращает: значение, ранг которого входит в допустимый интервал [target_rank - h, target_rank + h].
     */
    value_type select_value(Container& data, std::size_t target_rank, std::size_t n, std::size_t h, std::mt19937_64& rng) {
        auto begin = std::begin(data);

        const std::size_t a = h >= target_rank ? 0 : target_rank - h;
        const std::size_t distance_to_end = n - 1 - target_rank;
        const std::size_t b = h >= distance_to_end ? n - 1 : target_rank + h;

        std::size_t lo = 0;
        std::size_t hi = n - 1;

        while (true) {
            // Все ранги текущего диапазона входят в [a, b].
            if (a <= lo && hi <= b) {
                return *(begin + lo);
            }

            const auto pivot_index = random_index(lo, hi, rng);
            const value_type pivot = *(begin + pivot_index);

            const auto part = partition3(data, lo, hi, pivot);
            ++partitions_;

            // Блок равных pivot имеет нулевые ранги [lt, gt].
            if (part.lt <= b && a <= part.gt) {
                return *(begin + part.lt);
            }

            if (part.gt < a) {
                lo = part.gt + 1;
            } else {
                hi = part.lt - 1;
            }
        }
    }
};

} // namespace rapid_stats

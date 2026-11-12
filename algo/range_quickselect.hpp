#pragma once

#include "order_statistic_algorithm.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace rapid_stats {

template <typename Container>
class RangeQuickselect : public ApproximateOrderStatisticAlgorithm<Container> {
public:
    using Base = ApproximateOrderStatisticAlgorithm<Container>;
    using iterator = typename Base::iterator;
    using value_type = typename std::iterator_traits<iterator>::value_type;
    using difference_type = typename std::iterator_traits<iterator>::difference_type;

    static_assert(std::is_arithmetic_v<value_type> &&
                      !std::is_same_v<std::remove_cv_t<value_type>, bool>,
                  "RangeQuickselect requires an arithmetic value type");

    static_assert(std::is_base_of_v<std::random_access_iterator_tag,
                                    typename std::iterator_traits<iterator>::iterator_category>,
                  "RangeQuickselect requires random-access iterators");

    std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks, double eps,
                               [[maybe_unused]] double delta, std::uint64_t seed) override {
        /*
         * Автор: Viktor Hlyupin
         * Принимает: data — выборка, ranks — ранги от 1 до n, eps — доля n, delta — вероятность ошибки, seed — зерно.
         * Возвращает: итераторы статистик в порядке ranks. Пустой запрос не меняет массив.
         */
        partitions_ = 0;
        comparisons_ = 0;

        if (eps < 0.0 || eps > 1.0) {
            throw std::invalid_argument("eps must be in range [0, 1]");
        }

        if (ranks.empty()) {
            return {};
        }

        const std::size_t n = data.size();
        std::vector<Target> targets;
        targets.reserve(ranks.size());
        for (std::size_t index = 0; index < ranks.size(); ++index) {
            const std::size_t rank = ranks[index];
            if (rank == 0 || rank > n) {
                throw std::out_of_range("Rank must be in range [1, n]");
            }
            targets.push_back(Target{rank - 1, index});
        }

        if constexpr (std::is_floating_point_v<value_type>) {
            /*
             * Автор: Viktor Hlyupin
             * Принимает: ветвь для вещественного типа.
             * Возвращает: ничего; неконечные числа отклоняются до перестановок.
             */
            for (auto it = std::begin(data); it != std::end(data); ++it) {
                if (!std::isfinite(*it)) {
                    throw std::invalid_argument("Array must contain only finite numbers");
                }
            }
        }

        std::sort(targets.begin(), targets.end());

        std::vector<iterator> result(ranks.size());
        std::mt19937_64 rng(seed);
        select_many(data, 0, n - 1, targets, 0, targets.size(), result, rng);
        return result;
    }

    [[nodiscard]] std::uint64_t partitions() const noexcept {
        /*
         * Автор: Tatiana Pechnikova
         * Принимает: ничего.
         * Возвращает: число разбиений за последний вызов sort().
         */
        return partitions_;
    }

    [[nodiscard]] std::uint64_t comparisons() const noexcept {
        /*
         * Автор: Tatiana Pechnikova
         * Принимает: ничего.
         * Возвращает: число сравнений с опорным элементом за последний вызов sort().
         */
        return comparisons_;
    }

private:
    // Нулевой ранг и индекс этого ранга в исходном запросе.
    struct Target {
        std::size_t rank;
        std::size_t origin;

        bool operator<(const Target& other) const {
            /*
             * Автор: Viktor Hlyupin
             * Принимает: other — другая пара из нулевого ранга и индекса запроса.
             * Возвращает: истину, если эта пара идёт раньше other.
             */
            return rank < other.rank || (rank == other.rank && origin < other.origin);
        }
    };

    struct PartitionResult {
        std::size_t lt;
        std::size_t gt;
    };

    std::uint64_t partitions_ = 0;
    std::uint64_t comparisons_ = 0;

    std::size_t random_index(std::size_t lo, std::size_t hi, std::mt19937_64& rng) {
        /*
         * Автор: Tatiana Pechnikova
         * Принимает: lo и hi — границы диапазона включительно, rng — генератор.
         * Возвращает: случайный индекс из [lo, hi].
         */
        std::uniform_int_distribution<std::size_t> dist(lo, hi);
        return dist(rng);
    }

    PartitionResult partition3(Container& data, std::size_t lo, std::size_t hi,
                               const value_type& pivot) {
        /*
         * Автор: Tatiana Pechnikova
         * Принимает: data, lo, hi — границы диапазона, pivot — опорное значение.
         * Возвращает: границы блока элементов, равных pivot.
         */
        auto begin = std::begin(data);
        std::size_t lt = lo;
        std::size_t i = lo;
        std::size_t gt = hi;

        while (i <= gt) {
            ++comparisons_;
            if (*(begin + static_cast<difference_type>(i)) < pivot) {
                std::iter_swap(begin + static_cast<difference_type>(lt),
                               begin + static_cast<difference_type>(i));
                ++lt;
                ++i;
            } else {
                ++comparisons_;
                if (*(begin + static_cast<difference_type>(i)) > pivot) {
                    std::iter_swap(begin + static_cast<difference_type>(i),
                                   begin + static_cast<difference_type>(gt));
                    --gt;
                } else {
                    ++i;
                }
            }
        }
        return {lt, gt};
    }

    void record(Container& data, const std::vector<Target>& targets, std::size_t target_begin,
                std::size_t target_end, std::vector<iterator>& result) const {
        /*
         * Автор: Viktor Hlyupin
         * Принимает: data, срез targets [target_begin, target_end) и result.
         * Возвращает: ничего; для каждого ранга среза записывает итератор на его позицию.
         */
        const auto begin = std::begin(data);
        for (std::size_t index = target_begin; index < target_end; ++index) {
            result[targets[index].origin] =
                begin + static_cast<difference_type>(targets[index].rank);
        }
    }

    void select_many(Container& data, std::size_t lo, std::size_t hi,
                     const std::vector<Target>& targets, std::size_t target_begin,
                     std::size_t target_end, std::vector<iterator>& result, std::mt19937_64& rng) {
        /*
         * Автор: Viktor Hlyupin
         * Принимает: data, границы [lo, hi], отсортированные targets, их срез, result и rng.
         * Возвращает: ничего; закрывает ранги внутри блока равных и ищет остальные слева и справа.
         */
        while (target_begin < target_end && lo <= hi) {
            if (lo == hi) {
                record(data, targets, target_begin, target_end, result);
                return;
            }

            const value_type pivot =
                *(std::begin(data) + static_cast<difference_type>(random_index(lo, hi, rng)));
            const PartitionResult part = partition3(data, lo, hi, pivot);
            ++partitions_;

            const auto left_end_it = std::lower_bound(
                targets.begin() + static_cast<difference_type>(target_begin),
                targets.begin() + static_cast<difference_type>(target_end), Target{part.lt, 0});
            const auto right_begin_it =
                std::upper_bound(targets.begin() + static_cast<difference_type>(target_begin),
                                 targets.begin() + static_cast<difference_type>(target_end),
                                 Target{part.gt, std::numeric_limits<std::size_t>::max()});
            const std::size_t left_end = static_cast<std::size_t>(left_end_it - targets.begin());
            const std::size_t right_begin =
                static_cast<std::size_t>(right_begin_it - targets.begin());

            // Ранги внутри [lt, gt] уже стоят на местах. Один блок равных закрывает их все.
            record(data, targets, left_end, right_begin, result);

            const bool has_left = target_begin < left_end;
            const bool has_right = right_begin < target_end;
            if (has_left && has_right) {
                // Меньшая часть рекурсией, большая — циклом, поэтому стек O(log n).
                const std::size_t left_count = part.lt - lo;
                const std::size_t right_count = hi - part.gt;
                if (left_count < right_count) {
                    select_many(data, lo, part.lt - 1, targets, target_begin, left_end, result,
                                rng);
                    lo = part.gt + 1;
                    target_begin = right_begin;
                } else {
                    select_many(data, part.gt + 1, hi, targets, right_begin, target_end, result,
                                rng);
                    hi = part.lt - 1;
                    target_end = left_end;
                }
            } else if (has_left) {
                hi = part.lt - 1;
                target_end = left_end;
            } else if (has_right) {
                lo = part.gt + 1;
                target_begin = right_begin;
            } else {
                return;
            }
        }
    }
};

} // namespace rapid_stats

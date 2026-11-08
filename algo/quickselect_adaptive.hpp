#pragma once

#include "order_statistic_algorithm.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace rapid_stats {

template <typename Container>
class QuickselectAdaptive : public ExactOrderStatisticAlgorithm<Container> {
public:
    using Base = ExactOrderStatisticAlgorithm<Container>;
    using iterator = typename Base::iterator;
    using value_type = typename std::iterator_traits<iterator>::value_type;
    using difference_type = typename std::iterator_traits<iterator>::difference_type;

    static_assert(
        std::is_arithmetic_v<value_type> &&
            !std::is_same_v<std::remove_cv_t<value_type>, bool>,
        "QuickselectAdaptive requires an arithmetic value type");

    static_assert(
        std::is_base_of_v<
            std::random_access_iterator_tag,
            typename std::iterator_traits<iterator>::iterator_category>,
        "QuickselectAdaptive requires random-access iterators");

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: data — контейнер с числовыми значениями, ranks — запрошенные ранги (от 1 до n).
     * Возвращает: итераторы на элементы с заданными рангами в порядке ranks.
     */
    std::vector<iterator> sort(
        Container& data, const std::vector<std::size_t>& ranks) override {
        partitions_ = 0;
        comparisons_ = 0;

        if (data.empty()) {
            throw std::invalid_argument("Array must not be empty");
        }

        const std::size_t n = data.size();
        auto begin = std::begin(data);
        auto end = std::end(data);

        if constexpr (std::is_floating_point_v<value_type>) {
            for (auto it = begin; it != end; ++it) {
                if (!std::isfinite(*it)) {
                    throw std::invalid_argument(
                        "Array must contain only finite numbers");
                }
            }
        }

        std::vector<std::pair<std::size_t, std::size_t>> ordered_ranks;
        ordered_ranks.reserve(ranks.size());

        for (std::size_t i = 0; i < ranks.size(); ++i) {
            if (ranks[i] == 0 || ranks[i] > n) {
                throw std::out_of_range("Rank must be in range [1, n]");
            }
            // Переводим в 0-based для внутренней работы
            ordered_ranks.emplace_back(ranks[i] - 1, i);
        }

        std::sort(ordered_ranks.begin(), ordered_ranks.end());

        std::vector<iterator> result(ranks.size());

        std::size_t lo = 0;
        std::size_t previous_rank = n;

        for (const auto& [rank, original_index] : ordered_ranks) {
            if (rank != previous_rank) {
                select(data, rank, lo);
                lo = rank + 1;
                previous_rank = rank;
            }

            result[original_index] = begin + static_cast<difference_type>(rank);
        }

        return result;
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
    std::uint64_t partitions_ = 0;
    std::uint64_t comparisons_ = 0;

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: values — массив значений; rank — нулевой ранг.
     * Возвращает: значение с заданным рангом; массив переставляется.
     */
    static value_type deterministic_select(
        std::vector<value_type>& values, std::size_t rank) {
        std::size_t lo = 0;
        std::size_t hi = values.size();

        while (true) {
            if (hi - lo <= 16) {
                std::sort(
                    values.begin() + static_cast<std::ptrdiff_t>(lo),
                    values.begin() + static_cast<std::ptrdiff_t>(hi));
                return values[rank];
            }

            std::vector<value_type> medians;
            medians.reserve((hi - lo + 4) / 5);

            for (std::size_t first = lo; first < hi;) {
                const std::size_t last =
                    first + std::min<std::size_t>(5, hi - first);
                std::sort(
                    values.begin() + static_cast<std::ptrdiff_t>(first),
                    values.begin() + static_cast<std::ptrdiff_t>(last));
                medians.push_back(values[first + (last - first) / 2]);
                first = last;
            }

            const value_type pivot =
                deterministic_select(medians, medians.size() / 2);

            std::size_t less = lo;
            std::size_t current = lo;
            std::size_t greater = hi;

            while (current < greater) {
                if (values[current] < pivot) {
                    std::swap(values[less], values[current]);
                    ++less;
                    ++current;
                } else if (pivot < values[current]) {
                    --greater;
                    std::swap(values[current], values[greater]);
                } else {
                    ++current;
                }
            }

            if (rank < less) {
                hi = less;
            } else if (rank >= greater) {
                lo = greater;
            } else {
                return pivot;
            }
        }
    }

    /*
     * Автор: Tatiana Pechnikova
     * Принимает: data — контейнер, rank — искомый нулевой ранг, lo — нижняя граница активного диапазона.
     * Возвращает: ничего; элемент заданного ранга оказывается на своей позиции.
     */
    void select(Container& data, std::size_t rank, std::size_t lo) {
        auto begin = std::begin(data);
        std::size_t hi = data.size();

        while (true) {
            const std::size_t length = hi - lo;
            const std::size_t relative_rank = rank - lo;

            if (length <= 16) {
                std::sort(
                    begin + static_cast<difference_type>(lo),
                    begin + static_cast<difference_type>(hi));
                return;
            }

            if (relative_rank == 0) {
                auto first = begin + static_cast<difference_type>(lo);
                auto last = begin + static_cast<difference_type>(hi);
                std::iter_swap(first, std::min_element(first, last));
                return;
            }

            if (relative_rank == length - 1) {
                auto first = begin + static_cast<difference_type>(lo);
                auto last = begin + static_cast<difference_type>(hi);
                std::iter_swap(last - 1, std::max_element(first, last));
                return;
            }

            std::vector<value_type> candidates;
            candidates.reserve((length + 8) / 9);

            const bool use_minima = relative_rank <= length / 6;
            const bool use_maxima =
                !use_minima && relative_rank / 5 >= length - relative_rank;

            for (std::size_t first = lo; first < hi;) {
                const std::size_t last =
                    first + std::min<std::size_t>(9, hi - first);

                if (use_minima) {
                    value_type candidate =
                        *(begin + static_cast<difference_type>(first));
                    for (std::size_t i = first + 1; i < last; ++i) {
                        const value_type value =
                            *(begin + static_cast<difference_type>(i));
                        if (value < candidate) {
                            candidate = value;
                        }
                    }
                    candidates.push_back(candidate);
                } else if (use_maxima) {
                    value_type candidate =
                        *(begin + static_cast<difference_type>(first));
                    for (std::size_t i = first + 1; i < last; ++i) {
                        const value_type value =
                            *(begin + static_cast<difference_type>(i));
                        if (candidate < value) {
                            candidate = value;
                        }
                    }
                    candidates.push_back(candidate);
                } else {
                    std::vector<value_type> group;
                    group.reserve(last - first);
                    for (std::size_t i = first; i < last; ++i) {
                        group.push_back(*(begin + static_cast<difference_type>(i)));
                    }
                    std::sort(group.begin(), group.end());
                    candidates.push_back(group[group.size() / 2]);
                }

                first = last;
            }

            const value_type pivot =
                deterministic_select(candidates, candidates.size() / 2);

            std::size_t less = lo;
            std::size_t current = lo;
            std::size_t greater = hi;

            while (current < greater) {
                const value_type value =
                    *(begin + static_cast<difference_type>(current));

                ++comparisons_;

                if (value < pivot) {
                    std::iter_swap(
                        begin + static_cast<difference_type>(less),
                        begin + static_cast<difference_type>(current));
                    ++less;
                    ++current;
                } else {
                    ++comparisons_;
                    if (pivot < value) {
                        --greater;
                        std::iter_swap(
                            begin + static_cast<difference_type>(current),
                            begin + static_cast<difference_type>(greater));
                    } else {
                        ++current;
                    }
                }
            }

            ++partitions_;

            if (rank < less) {
                hi = less;
            } else if (rank >= greater) {
                lo = greater;
            } else {
                return;
            }
        }
    }
};

}  // namespace rapid_stats

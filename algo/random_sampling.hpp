// Файл реализации алгоритма приближённого нахождения k-статистик методом Random sampling
// Класс RandomSamplingAlgorithm наследуется от OrderStatisticAlgorithm
// пример использования:

// std::vector<int> data = {9, 2, 7, 1, 5, 3, 8};
// std::vector<std::size_t> ranks = {2, 5};
// auto exact_selector =[](std::vector<int>& values, std::size_t k) {
//         return floyd_rivest_select(values, k);};
// rapid_stats::RandomSamplingAlgorithm<std::vector<int>, decltype(exact_selector)> algorithm(
//     data,
//     1,       // h
//     0.1,     // delta
//     12345,   // seed
//     exact_selector
// );
// auto result = algorithm.sort(ranks);
// возвращается объект типа std::vector<iterator> - найденный статистики

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

template <typename Container, typename SelectExact>
class RandomSamplingAlgorithm: public OrderStatisticAlgorithm<Container> {
private:
    using Base = OrderStatisticAlgorithm<Container>;

public:
    using iterator = typename Base::iterator;
    using value_type =
        typename std::iterator_traits<iterator>::value_type;

    RandomSamplingAlgorithm(
        Container& data,
        std::size_t h,
        double delta,
        std::uint64_t seed,
        SelectExact select_exact
    )
        : Base(data),
          h_(h),
          delta_(delta),
          seed_(seed),
          select_exact_(std::move(select_exact)) {

        if (!std::isfinite(delta_) ||
            delta_ <= 0.0 ||
            delta_ >= 1.0) {
            throw std::invalid_argument(
                "delta must be in (0, 1)"
            );
        }
    }

    std::vector<iterator> sort(
        const std::vector<std::size_t>& ranks
    ) override {
        const auto distance = std::distance(
            this->begin_,
            this->end_
        );

        if (distance < 0) {
            throw std::invalid_argument(
                "Invalid container range"
            );
        }

        const std::size_t n =
            static_cast<std::size_t>(distance);

        for (std::size_t rank : ranks) {
            if (rank >= n) {
                throw std::out_of_range(
                    "Rank is out of range"
                );
            }
        }

        std::vector<iterator> result;
        result.reserve(ranks.size());

        if (ranks.empty()) {
            return result;
        }

        // Копируем элементы исходного контейнера.
        std::vector<value_type> data(
            this->begin_,
            this->end_
        );

        for (std::size_t rank : ranks) {
            value_type selected = random_sampling_select(
                data,
                rank,
                h_,
                delta_,
                seed_,
                select_exact_
            );

            // Находим в исходном контейнере элемент
            // со значением, которое вернул алгоритм.
            auto it = std::find_if(
                this->begin_,
                this->end_,
                [&](const auto& value) {
                    return value == selected;
                }
            );

            result.push_back(it);
        }

        return result;
    }

private:
    std::size_t h_;
    double delta_;
    std::uint64_t seed_;
    SelectExact select_exact_;

    template <typename T, typename ExactSelector>
    static T random_sampling_select(
        std::vector<T>& a,
        std::size_t k,
        std::size_t h,
        double delta,
        std::uint64_t seed,
        ExactSelector select_exact
    ) {
        const std::size_t n = a.size();

        if (n == 0) {
            throw std::invalid_argument(
                "Array must not be empty"
            );
        }

        if (k >= n) {
            throw std::out_of_range(
                "k is out of range"
            );
        }

        if (h == 0) {
            return select_exact(a, k);
        }

        const long double epsilon =
            static_cast<long double>(h) / n;

        const long double log_term =
            std::log(2.0L) -
            std::log(static_cast<long double>(delta));

        const long double raw_m =
            log_term / (2.0L * epsilon * epsilon);

        if (!std::isfinite(raw_m) ||
            raw_m >= static_cast<long double>(n)) {
            return select_exact(a, k);
        }

        const std::size_t m =
            static_cast<std::size_t>(std::ceil(raw_m));

        if (m == 0) {
            throw std::runtime_error(
                "Calculated sample size is invalid"
            );
        }

        const std::size_t requested_rank = k + 1;

        const long double raw_j = std::ceil(
            static_cast<long double>(m) *
            static_cast<long double>(requested_rank) /
            static_cast<long double>(n)
        );

        const std::size_t j =
            static_cast<std::size_t>(raw_j);

        std::mt19937_64 rng(seed);

        std::uniform_int_distribution<std::size_t>
            index_dist(0, n - 1);

        std::vector<T> sample;
        sample.reserve(m);

        for (std::size_t i = 0; i < m; ++i) {
            sample.push_back(a[index_dist(rng)]);
        }

        return select_exact(sample, j - 1);
    }
};

} // namespace rapid_stats
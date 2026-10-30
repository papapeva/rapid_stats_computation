#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

#include "order_statistic_algorithm.hpp"

namespace rapid_stats {

template <typename Container>
class ApproxMedianAlgorithm : public OrderStatisticAlgorithm<Container> {
public:
    using Base = OrderStatisticAlgorithm<Container>;
    using iterator = typename Base::iterator;
    using value_type = typename Container::value_type;

    // epsilon — допустимая ошибка ранга в долях от n.
    // c — параметр надежности из оценки вероятности 1 - n^(-c).
    // seed — seed генератора случайных чисел.
    explicit ApproxMedianAlgorithm(
        Container& data,
        double epsilon = 0.1,
        double c = 1.0,
        std::uint64_t seed = 123456789ULL)
        : Base(data),
          epsilon_(epsilon),
          c_(c),
          seed_(seed) {
        if (!(epsilon_ > 0.0 && epsilon_ < 1.0)) {
            throw std::invalid_argument("epsilon must be in (0, 1)");
        }
        if (!(c_ > 0.0) || !std::isfinite(c_)) {
            throw std::invalid_argument("c must be positive and finite");
        }
    }

    std::vector<iterator> sort(
        const std::vector<std::size_t>& ranks) override {

        const auto n_signed = std::distance(this->begin_, this->end_);
        if (n_signed < 0) {
            throw std::invalid_argument("Invalid iterator range");
        }

        const std::size_t n = static_cast<std::size_t>(n_signed);

        if (n == 0) {
            if (!ranks.empty()) {
                throw std::out_of_range(
                    "Cannot select a rank from an empty container");
            }
            return {};
        }

        for (const std::size_t k : ranks) {
            if (k >= n) {
                throw std::out_of_range("Rank must be in [0, n)");
            }
        }

        std::vector<iterator> result;
        result.reserve(ranks.size());

        if (ranks.empty()) {
            return result;
        }

        if (n == 1) {
            result.assign(ranks.size(), this->begin_);
            return result;
        }

        // Вычисляем размер выборки:
        // s = min(n, ceil(4*c*ln(n)/epsilon^2)).
        const long double raw_sample_size =
            4.0L * static_cast<long double>(c_) *
            std::log(static_cast<long double>(n)) /
            (static_cast<long double>(epsilon_) * epsilon_);

        std::size_t sample_size = n;

        if (std::isfinite(raw_sample_size) &&
            raw_sample_size < static_cast<long double>(n)) {
            sample_size = static_cast<std::size_t>(
                std::ceil(raw_sample_size));
            if (sample_size == 0) {
                sample_size = 1;
            }
        }

        // Если выборка покрывает весь массив, точный результат.
        if (sample_size >= n) {
            std::vector<iterator> all;
            all.reserve(n);

            for (auto it = this->begin_; it != this->end_; ++it) {
                all.push_back(it);
            }

            std::sort(all.begin(), all.end(),
                [](iterator a, iterator b) {
                    return *a < *b;
                });

            for (const std::size_t k : ranks) {
                result.push_back(all[k]);
            }
            return result;
        }

        // Reservoir sampling: равновероятная выборка sample_size
        // элементов без повторений из произвольного диапазона.
        std::mt19937_64 rng(seed_);
        std::vector<iterator> sample;
        sample.reserve(sample_size);

        std::size_t index = 0;
        for (auto it = this->begin_; it != this->end_; ++it, ++index) {
            if (index < sample_size) {
                sample.push_back(it);
            } else {
                std::uniform_int_distribution<std::size_t> dist(0, index);
                const std::size_t j = dist(rng);
                if (j < sample_size) {
                    sample[j] = it;
                }
            }
        }

        // Сортируем только выборку.
        std::sort(sample.begin(), sample.end(),
            [](iterator a, iterator b) {
                return *a < *b;
            });

        // Переводим исходный ранг в относительный ранг выборки.
        // Нулевые ранги: [0, n-1].
        for (const std::size_t k : ranks) {
            // целевая доля
            const long double q =
                static_cast<long double>(k) / static_cast<long double>(n);

            // ранг в выборке: округляем к ближайшему, а не floor
            long double pos = q * static_cast<long double>(sample_size - 1);
            std::size_t sample_rank = static_cast<std::size_t>(std::llround(pos));

            if (sample_rank >= sample_size) sample_rank = sample_size - 1;
            result.push_back(sample[sample_rank]);
        }

        return result;
    }

private:
    double epsilon_;
    double c_;
    std::uint64_t seed_;
};

} // namespace rapid_stats

#pragma once

#include "order_statistic_algorithm.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rapid_stats {

namespace detail {

template <typename Iterator> bool approx_iterator_less(const Iterator& lhs, const Iterator& rhs) {
    /*
     * Автор: Valeriya Sokolova
     * Принимает: lhs и rhs — итераторы сравниваемых элементов.
     * Возвращает: истину, если *lhs меньше *rhs.
     */
    return *lhs < *rhs;
}

} // namespace detail

// Приближённая медиана по случайной выборке. Возвращает один элемент исходного контейнера.
template <typename Container>
class ApproxMedianAlgorithm : public ApproximateOrderStatisticAlgorithm<Container> {
public:
    using iterator = typename ApproximateOrderStatisticAlgorithm<Container>::iterator;

    std::vector<iterator> sort(Container& data, std::size_t rank, double eps, double delta,
                               std::uint64_t seed) override {
        /*
         * Автор: Valeriya Sokolova
         * Принимает: data — выборка, rank — ранг от 1 до n, eps — доля n, delta — вероятность ошибки, seed — зерно.
         * Возвращает: вектор из одного итератора на элемент выборки.
         */
        if (!(eps > 0.0 && eps < 1.0)) {
            throw std::invalid_argument("eps must be in (0, 1)");
        }
        if (!(delta > 0.0 && delta < 1.0) || !std::isfinite(delta)) {
            throw std::invalid_argument("delta must be in (0, 1)");
        }

        const auto distance = std::distance(std::begin(data), std::end(data));

        if (distance < 0) {
            throw std::invalid_argument("Invalid iterator range");
        }

        const std::size_t n = static_cast<std::size_t>(distance);

        if (n == 0 || rank < 1 || rank > n) {
            throw std::out_of_range("Rank must be in [1, n]");
        }

        const std::size_t index = rank - 1;

        if (n == 1) {
            return {std::begin(data)};
        }

        // Надёжность c = ln(1 / delta) / ln(n), размер выборки 4 * c * ln(n) / eps^2.
        const long double raw_sample_size =
            4.0L * std::log(1.0L / static_cast<long double>(delta)) /
            (static_cast<long double>(eps) * static_cast<long double>(eps));

        std::size_t sample_size = n;

        if (std::isfinite(raw_sample_size) && raw_sample_size < static_cast<long double>(n)) {
            sample_size = static_cast<std::size_t>(std::ceil(raw_sample_size));

            if (sample_size == 0) {
                sample_size = 1;
            }
        }

        std::vector<iterator> sample;
        sample.reserve(sample_size);

        if (sample_size >= n) {
            for (auto it = std::begin(data); it != std::end(data); ++it) {
                sample.push_back(it);
            }
        } else {
            std::mt19937_64 rng(seed);
            std::size_t seen = 0;

            for (auto it = std::begin(data); it != std::end(data); ++it, ++seen) {
                if (seen < sample_size) {
                    sample.push_back(it);
                } else {
                    std::uniform_int_distribution<std::size_t> dist(0, seen);
                    const std::size_t pick = dist(rng);

                    if (pick < sample_size) {
                        sample[pick] = it;
                    }
                }
            }
        }

        std::sort(sample.begin(), sample.end(), detail::approx_iterator_less<iterator>);

        std::size_t sample_rank = index;

        if (sample_size < n) {
            const long double position = static_cast<long double>(index) /
                                         static_cast<long double>(n) *
                                         static_cast<long double>(sample_size - 1);
            sample_rank = static_cast<std::size_t>(std::llround(position));

            if (sample_rank >= sample_size) {
                sample_rank = sample_size - 1;
            }
        }

        return {sample[sample_rank]};
    }
};

} // namespace rapid_stats

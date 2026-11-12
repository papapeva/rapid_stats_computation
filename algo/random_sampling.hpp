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

// Приближённый поиск случайной выборкой. Точный добор делает nth_element на копии.
template <typename Container>
class RandomSamplingAlgorithm : public ApproximateOrderStatisticAlgorithm<Container> {
public:
    using iterator = typename ApproximateOrderStatisticAlgorithm<Container>::iterator;
    using value_type = typename std::iterator_traits<iterator>::value_type;

    std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks, double eps,
                               double delta, std::uint64_t seed) override {
        /*
         * Автор: Valeriya Sokolova
         * Принимает: data — выборка, ranks — ранги от 1 до n, eps — доля n, delta — вероятность ошибки, seed — зерно.
         * Возвращает: итераторы приближённых статистик в порядке ranks.
         */
        if (!(eps > 0.0 && eps < 1.0) || !std::isfinite(eps)) {
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

        if (ranks.empty()) {
            return {};
        }

        for (const std::size_t rank : ranks) {
            if (n == 0 || rank < 1 || rank > n) {
                throw std::out_of_range("Rank must be in [1, n]");
            }
        }

        const std::vector<value_type> values(std::begin(data), std::end(data));

        std::mt19937_64 rng(seed);

        std::vector<iterator> result;
        result.reserve(ranks.size());

        for (const std::size_t rank : ranks) {
            const value_type selected = sample(values, rank, eps, delta, rng);

            iterator found = std::begin(data);
            const iterator end = std::end(data);

            while (found != end &&
                   (*found < selected || selected < *found)) {
                ++found;
            }

            if (found == end) {
                throw std::logic_error("Selected value is missing from the container");
            }

            result.push_back(found);
        }

        return result;
    }

private:
    static value_type exact_at(std::vector<value_type>& values, std::size_t index) {
        /*
         * Автор: Valeriya Sokolova
         * Принимает: values — копия выборки, index — нулевой ранг.
         * Возвращает: элемент этого ранга.
         */
        const auto nth = values.begin() + static_cast<std::ptrdiff_t>(index);
        std::nth_element(values.begin(), nth, values.end());
        return *nth;
    }

    static value_type sample(const std::vector<value_type>& values, std::size_t rank, double eps,
                             double delta, std::mt19937_64& rng) {
        /*
         * Автор: Valeriya Sokolova
         * Принимает: values — копия выборки, rank — ранг от 1 до n, eps — доля n, delta — вероятность ошибки, seed — зерно.
         * Возвращает: значение приближённой порядковой статистики.
         */
        const std::size_t n = values.size();
        const std::size_t index = rank - 1;

        const long double epsilon = static_cast<long double>(eps);
        const long double log_term = std::log(2.0L) - std::log(static_cast<long double>(delta));

        const long double raw_m = log_term / (2.0L * epsilon * epsilon);

        if (!std::isfinite(raw_m) || raw_m >= static_cast<long double>(n)) {
            std::vector<value_type> working = values;
            return exact_at(working, index);
        }

        const std::size_t m = static_cast<std::size_t>(std::ceil(raw_m));

        if (m == 0) {
            throw std::runtime_error("Calculated sample size is invalid");
        }

        if (m >= n) {
            std::vector<value_type> working = values;
            return exact_at(working, index);
        }

        const long double raw_j =
            std::ceil(static_cast<long double>(m) * static_cast<long double>(rank) / static_cast<long double>(n));

        std::size_t j = static_cast<std::size_t>(raw_j);

        if (j < 1) {
            j = 1;
        }
        if (j > m) {
            j = m;
        }

        std::uniform_int_distribution<std::size_t> index_dist(0, n - 1);

        std::vector<value_type> picked;
        picked.reserve(m);

        for (std::size_t i = 0; i < m; ++i) {
            picked.push_back(values[index_dist(rng)]);
        }

        return exact_at(picked, j - 1);
    }
};

} // namespace rapid_stats

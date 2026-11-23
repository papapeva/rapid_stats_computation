#pragma once

#include "order_statistic_algorithm.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <random>
#include <stdexcept>
#include <thread>
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
                               double delta, std::uint64_t seed, ExactOrderStatisticAlgorithm<Container>* exact_algorithm = nullptr) override {
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

        // Заранее выделяем позиции для результатов.
        std::vector<iterator> result(ranks.size());

        // Обработка одного ранга.
        // Каждая задача получает собственный генератор случайных чисел.
        auto process_rank = [&](std::size_t i) {
            std::mt19937_64 rng(seed + static_cast<std::uint64_t>(i));

            const value_type selected = sample(values, ranks[i], eps, delta, rng, exact_algorithm);

            iterator found = std::begin(data);
            const iterator end = std::end(data);

            while (found != end && (*found < selected || selected < *found)) {
                ++found;
            }

            if (found == end) {
                throw std::logic_error("Selected value is missing from the container");
            }

            // Каждая задача записывает результат в свою позицию.
            result[i] = found;
        };

        // Счётчик дополнительных потоков.
        std::atomic<int> active_threads(0);

        // Максимум 3 дополнительных потока + основной.
        const int max_additional_threads = 3;

        std::vector<std::thread> workers;
        std::vector<std::exception_ptr> worker_errors(ranks.size());

        std::exception_ptr main_error = nullptr;

        for (std::size_t i = 0; i < ranks.size(); ++i) {
            // Если удалось получить разрешение, запускаем отдельный поток.
            if (detail::try_acquire_thread(active_threads, max_additional_threads)) {
                try {
                    workers.emplace_back([&, i]() {
                        try {
                            process_rank(i);
                        } catch (...) {
                            worker_errors[i] = std::current_exception();
                        }

                        detail::release_thread(active_threads);
                    });
                } catch (...) {
                    // Поток создать не удалось.
                    detail::release_thread(active_threads);

                    // Выполняем эту задачу в основном потоке.
                    try {
                        process_rank(i);
                    } catch (...) {
                        main_error = std::current_exception();
                        break;
                    }
                }
            } else {
                // Нет свободного разрешения — работаем в основном потоке.
                try {
                    process_rank(i);
                } catch (...) {
                    main_error = std::current_exception();
                    break;
                }
            }
        }

        // Дожидаемся завершения всех запущенных потоков.
        for (std::thread& worker : workers) {
            worker.join();
        }

        // Передаём исключение из основного потока, если оно возникло.
        if (main_error) {
            std::rethrow_exception(main_error);
        }

        // Проверяем, не возникли ли исключения в дополнительных потоках.
        for (const std::exception_ptr& error : worker_errors) {
            if (error) {
                std::rethrow_exception(error);
            }
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
                             double delta, std::mt19937_64& rng, ExactOrderStatisticAlgorithm<std::vector<value_type>>* exact_algorithm) {
        /*
         * Автор: Valeriya Sokolova
         * Принимает: values — копия выборки, rank — ранг от 1 до n, eps — доля n, delta — вероятность ошибки, rng — генератор.
         * Возвращает: значение порядковой статистики.
         */
        const std::size_t n = values.size();
        const std::size_t index = rank - 1;

        const long double epsilon = static_cast<long double>(eps);
        const long double log_term = std::log(2.0L) - std::log(static_cast<long double>(delta));

        const long double raw_m = log_term / (2.0L * epsilon * epsilon);

        if (!std::isfinite(raw_m) || raw_m >= static_cast<long double>(n)) {
            std::vector<value_type> working = values;
            
            if (exact_algorithm != nullptr) {
                return *exact_algorithm->sort(working, {rank + 1})[0];
            }

            // Если алгоритм не передан:
            return exact_at(working, rank);
        }

        const std::size_t m = static_cast<std::size_t>(std::ceil(raw_m));

        if (m == 0) {
            throw std::runtime_error("Calculated sample size is invalid");
        }

        if (m >= n) {
            std::vector<value_type> working = values;
            
            if (exact_algorithm != nullptr) {
                return *exact_algorithm->sort(working, {rank + 1})[0];
            }

            // Если алгоритм не передан:
            return exact_at(working, rank);
        }

        const long double raw_j =
            std::ceil(static_cast<long double>(m) * static_cast<long double>(rank) /
                      static_cast<long double>(n));

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

        if (exact_algorithm != nullptr) {
            return *exact_algorithm->sort(picked, {j})[0];
        }

        // Если алгоритм не передан:
        return exact_at(picked, j - 1);
    }
};

} // namespace rapid_stats

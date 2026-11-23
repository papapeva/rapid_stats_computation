#pragma once

#include "order_statistic_algorithm.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>
#include <atomic>
#include <thread>
#include <exception>

namespace rapid_stats {

namespace detail {

bool try_acquire_thread(std::atomic<int>& active_threads, int max_threads) {
    int current = active_threads;

    while (current < max_threads) {
        if (active_threads.compare_exchange_weak(current, current + 1)) {
            return true;
        }
    }

    return false;
}

void release_thread(std::atomic<int>& active_threads) {
    int current = active_threads;

    while (!active_threads.compare_exchange_weak(current, current - 1)) {
    }
}

// Границы диапазона элементов, эквивалентных опорному.
struct Partition {
    std::size_t equal_begin;
    std::size_t equal_end;
};

template <typename Iterator> bool iterator_less(const Iterator& lhs, const Iterator& rhs) {
    /*
     * Автор: Valeriya Sokolova
     * Принимает: lhs и rhs — итераторы сравниваемых элементов.
     * Возвращает: истину, если *lhs меньше *rhs.
     */
    return *lhs < *rhs;
}

// Выбор элемента ранга index в диапазоне [left, right]. Индекс нулевой.
template <typename T, typename Compare>
Partition floyd_rivest_select(std::vector<T>& data, std::size_t left, std::size_t right,
                              std::size_t index, Compare comp) {
    /*
     * Автор: Valeriya Sokolova
     * Принимает: data — элементы, left и right — границы, index — нулевой ранг, comp — сравнение.
     * Возвращает: границы группы элементов, эквивалентных выбранному.
     */
    while (right > left) {
        const std::size_t n = right - left + 1;

        if (n > 600) {
            const double i = static_cast<double>(index - left + 1);
            const double size = static_cast<double>(n);
            const double z = std::log(size);
            const double s = 0.5 * std::exp(2.0 * z / 3.0);

            double sd = 0.5 * std::sqrt(z * s * (size - s) / size);

            if (i < size / 2.0) {
                sd = -sd;
            }

            const double sample_left_value =
                std::max(static_cast<double>(left),
                         std::min(static_cast<double>(index),
                                  std::floor(static_cast<double>(index) - i * s / size + sd)));
            const double sample_right_value = std::min(
                static_cast<double>(right),
                std::max(static_cast<double>(index),
                         std::floor(static_cast<double>(index) + (size - i) * s / size + sd)));

            const std::size_t sample_left = static_cast<std::size_t>(sample_left_value);
            const std::size_t sample_right = static_cast<std::size_t>(sample_right_value);

            floyd_rivest_select(data, sample_left, sample_right, index, comp);
        }

        const T pivot = data[index];

        std::size_t lt = left;
        std::size_t i = left;
        std::size_t gt = right + 1;

        while (i < gt) {
            if (comp(data[i], pivot)) {
                std::swap(data[lt], data[i]);
                ++lt;
                ++i;
            } else if (comp(pivot, data[i])) {
                --gt;
                std::swap(data[i], data[gt]);
            } else {
                ++i;
            }
        }

        if (index < lt) {
            right = lt - 1;
        } else if (index >= gt) {
            left = gt;
        } else {
            return {lt, gt - 1};
        }
    }

    return {left, right};
}

// indexes отсортирован по возрастанию и содержит нулевые позиции.
template <typename T, typename Compare>
void select_multiple(std::vector<T>& data, const std::vector<std::size_t>& indexes,
                     std::size_t index_begin, std::size_t index_end, std::size_t left,
                     std::size_t right, Compare comp,
                     std::atomic<int>& active_threads, int max_additional_threads) {
    /*
     * Автор: Valeriya Sokolova
     * Принимает: data — элементы, indexes — нулевые ранги, границы рангов и диапазона, comp — сравнение.
     * Возвращает: ничего. На позиции каждого запрошенного ранга ставит соответствующий элемент.
     */
    if (index_begin >= index_end || left >= right) {
        return;
    }

    const std::size_t middle = index_begin + (index_end - index_begin) / 2;
    const std::size_t index = indexes[middle];

    const Partition partition = floyd_rivest_select(data, left, right, index, comp);

    const auto first_right = std::lower_bound(
        indexes.begin() + static_cast<std::ptrdiff_t>(index_begin),
        indexes.begin() + static_cast<std::ptrdiff_t>(index_end), partition.equal_begin);

    const auto first_after_equal = std::upper_bound(
        indexes.begin() + static_cast<std::ptrdiff_t>(index_begin),
        indexes.begin() + static_cast<std::ptrdiff_t>(index_end), partition.equal_end);

    const std::size_t left_index_end = static_cast<std::size_t>(first_right - indexes.begin());
    const std::size_t right_index_begin =
        static_cast<std::size_t>(first_after_equal - indexes.begin());

    const bool has_left =
        left < partition.equal_begin && index_begin < left_index_end;

    const bool has_right =
        partition.equal_end < right && right_index_begin < index_end;

    // Пробуем запустить левую ветвь в дополнительном потоке.
    if (has_left && has_right &&
        try_acquire_thread(active_threads, max_additional_threads)) {

        std::exception_ptr worker_error = nullptr;
        std::thread worker;

        try {
            worker = std::thread([&]() {
                try {
                    select_multiple(data, indexes, index_begin, left_index_end,
                                    left, partition.equal_begin - 1, comp,
                                    active_threads, max_additional_threads);
                } catch (...) {
                    worker_error = std::current_exception();
                }

                release_thread(active_threads);
            });
        } catch (...) {
            // Если поток создать не удалось, освобождаем разрешение.
            release_thread(active_threads);

            // Выполняем обе ветви последовательно.
            select_multiple(data, indexes, index_begin, left_index_end,
                            left, partition.equal_begin - 1, comp,
                            active_threads, max_additional_threads);

            select_multiple(data, indexes, right_index_begin, index_end,
                            partition.equal_end + 1, right, comp,
                            active_threads, max_additional_threads);

            return;
        }

        std::exception_ptr main_error = nullptr;

        try {
            // Правая ветвь выполняется в основном потоке.
            select_multiple(data, indexes, right_index_begin, index_end,
                            partition.equal_end + 1, right, comp,
                            active_threads, max_additional_threads);
        } catch (...) {
            main_error = std::current_exception();
        }

        // Обязательно дожидаемся дополнительного потока.
        worker.join();

        if (main_error) {
            std::rethrow_exception(main_error);
        }

        if (worker_error) {
            std::rethrow_exception(worker_error);
        }

        return;
    }

    // Если дополнительный поток не получен,
    // выполняем ветви последовательно.
    if (has_left) {
        select_multiple(data, indexes, index_begin, left_index_end,
                        left, partition.equal_begin - 1, comp,
                        active_threads, max_additional_threads);
    }

    if (has_right) {
        select_multiple(data, indexes, right_index_begin, index_end,
                        partition.equal_end + 1, right, comp,
                        active_threads, max_additional_threads);
    }
}

} // namespace detail

// Точный поиск Floyd–Rivest. Исходный контейнер не переставляет.
template <typename Container>
class FloydRivestSelect : public ExactOrderStatisticAlgorithm<Container> {
public:
    using iterator = typename ExactOrderStatisticAlgorithm<Container>::iterator;

    std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Valeriya Sokolova
         * Принимает: data — выборка, ranks — ранги от 1 до n.
         * Возвращает: итераторы статистик в порядке ranks.
         */
        const auto distance = std::distance(std::begin(data), std::end(data));

        if (distance < 0) {
            throw std::invalid_argument("Invalid iterator range");
        }

        const std::size_t n = static_cast<std::size_t>(distance);

        if (ranks.empty()) {
            return {};
        }

        std::vector<std::size_t> indexes;
        indexes.reserve(ranks.size());

        for (const std::size_t rank : ranks) {
            if (rank < 1 || rank > n) {
                throw std::out_of_range("Rank must be in [1, n]");
            }
            indexes.push_back(rank - 1);
        }

        std::vector<iterator> elements;
        elements.reserve(n);

        for (auto it = std::begin(data); it != std::end(data); ++it) {
            elements.push_back(it);
        }

        std::vector<std::size_t> ordered = indexes;
        std::sort(ordered.begin(), ordered.end());
        ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());

        // Счётчик дополнительных потоков.
        std::atomic<int> active_threads(0);

        // Максимум 3 дополнительных потока + основной.
        const int max_additional_threads = 3;

        detail::select_multiple(elements, ordered, 0, ordered.size(), 0, n - 1,
                                detail::iterator_less<iterator>,
                                active_threads, max_additional_threads);

        std::vector<iterator> result;
        result.reserve(indexes.size());

        for (const std::size_t index : indexes) {
            result.push_back(elements[index]);
        }

        return result;
    }
};

} // namespace rapid_stats

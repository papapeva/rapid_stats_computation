#pragma once

#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace rapid_stats {

// Базовый класс поиска порядковых статистик. Контейнер не копируется.
template <typename Container> class OrderStatisticAlgorithm {
public:
    using iterator = decltype(std::begin(std::declval<Container&>()));

    explicit OrderStatisticAlgorithm(Container& data)
        : begin_(std::begin(data)), end_(std::end(data)) {
        /*
         * Автор: Victor Khlyupin
         * Принимает: data — выборка, границы которой запоминает алгоритм.
         * Возвращает: ничего.
         */
    }

    virtual ~OrderStatisticAlgorithm() = default;

    // ranks — нулевые ранги. Размер и порядок результата совпадают с ranks:
    // result[i] указывает на статистику ранга ranks[i].
    virtual std::vector<iterator> sort(const std::vector<std::size_t>& ranks) = 0;

protected:
    iterator begin_;
    iterator end_;
};

} // namespace rapid_stats

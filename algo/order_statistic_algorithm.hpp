#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

namespace rapid_stats {

// Корень алгоритмов порядковых статистик. Снаружи не создаётся.
// Ранг с единицы: 1 — минимум, n — максимум. Индекс элемента — rank - 1.
template <typename Container> class OrderStatisticAlgorithm {
public:
    using iterator = decltype(std::begin(std::declval<Container&>()));

    virtual ~OrderStatisticAlgorithm() = default;

protected:
    OrderStatisticAlgorithm() = default;
};

// Точный поиск. ranks — ранги от 1 до n, result[i] — статистика ранга ranks[i].
// Контейнер не копируется, алгоритм может переставлять элементы.
template <typename Container>
class ExactOrderStatisticAlgorithm : public OrderStatisticAlgorithm<Container> {
public:
    using iterator = typename OrderStatisticAlgorithm<Container>::iterator;

    virtual std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks) = 0;
};

// Приближённый поиск. ranks — ранги от 1 до n, result[i] — приближение статистики ranks[i].
// eps — доля n, допустимое отклонение ранга. delta — вероятность ошибки.
// seed задаёт воспроизводимость.
template <typename Container>
class ApproximateOrderStatisticAlgorithm : public OrderStatisticAlgorithm<Container> {
public:
    using iterator = typename OrderStatisticAlgorithm<Container>::iterator;

    virtual std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks,
                                       double eps, double delta, std::uint64_t seed) = 0;
};

} // namespace rapid_stats

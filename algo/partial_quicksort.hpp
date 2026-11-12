#pragma once

#include "detail/partial_quicksort_impl.hpp"
#include "detail/vector_contract.hpp"

#include <algorithm>

namespace rapid_stats {

template <typename Container>
class PartialQuicksortAlgorithm : public ExactOrderStatisticAlgorithm<Container> {
public:
    using iterator = typename ExactOrderStatisticAlgorithm<Container>::iterator;

    explicit PartialQuicksortAlgorithm(std::uint64_t seed = 20261105) : seed_(seed) {
        /*
         * Автор: Polikarpov Kirill
         * Принимает: seed выбора опорных элементов.
         * Возвращает: объект Partial Quicksort без копирования массива.
         */
    }

    std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Polikarpov Kirill
         * Принимает: массив и ранги с единицы; пустой запрос ничего не меняет.
         * Возвращает: итераторы на статистики; префикс до максимального ранга включительно отсортирован.
         */
        detail::validate_ranks(data, ranks);
        if (!ranks.empty()) {
            sort_prefix(data, *std::max_element(ranks.begin(), ranks.end()));
        }
        return detail::rank_iterators(data, ranks);
    }

    void sort_prefix(Container& data, std::size_t count) {
        /*
         * Автор: Polikarpov Kirill
         * Принимает: количество наименьших элементов count от 0 до size, НЕ ранг.
         * Возвращает: ничего; сортирует префикс, count=0 ничего не меняет, count=size сортирует всё.
         */
        detail::partial::partial_quicksort(data, count, seed_);
    }

private:
    std::uint64_t seed_;
};
} // namespace rapid_stats

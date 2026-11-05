#pragma once

#include "detail/partial_quicksort_impl.hpp"
#include "detail/vector_contract.hpp"

#include <algorithm>

namespace rapid_stats {

template <typename Container>
class PartialQuicksortAlgorithm : public OrderStatisticAlgorithm<Container> {
public:
    using iterator = typename OrderStatisticAlgorithm<Container>::iterator;

    explicit PartialQuicksortAlgorithm(Container& data, std::uint64_t seed = 20261105)
        : OrderStatisticAlgorithm<Container>(data), data_(data), seed_(seed) {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: изменяемый std::vector и seed выбора опорных элементов.
         * Возвращает: объект Partial Quicksort без копирования массива.
         */
    }

    std::vector<iterator> sort(const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: нулевые ранги; пустой запрос ничего не меняет.
         * Возвращает: итераторы на статистики; префикс до максимального ранга включительно отсортирован.
         */
        detail::validate_ranks(data_, ranks);
        if (!ranks.empty()) {
            sort_prefix(*std::max_element(ranks.begin(), ranks.end()) + 1);
        }
        return detail::rank_iterators(data_, ranks);
    }

    void sort_prefix(std::size_t count) {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: количество наименьших элементов count от 0 до size, НЕ ранг.
         * Возвращает: ничего; сортирует префикс, count=0 ничего не меняет, count=size сортирует всё.
         */
        detail::partial::partial_quicksort(data_, count, seed_);
    }

private:
    Container& data_;
    std::uint64_t seed_;
};
} // namespace rapid_stats

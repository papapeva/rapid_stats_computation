#pragma once

#include "detail/radix_impl.hpp"
#include "detail/vector_contract.hpp"

#include <algorithm>

namespace rapid_stats {

template <typename Container>
class RadixSelectAlgorithm : public ExactOrderStatisticAlgorithm<Container> {
public:
    using iterator = typename ExactOrderStatisticAlgorithm<Container>::iterator;

    RadixSelectAlgorithm() = default;

    std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: массив и ранги с единицы; допускаются повторы и произвольный порядок.
         * Возвращает: итераторы на найденные значения в исходном контейнере, не обязательно begin+(rank-1).
         */
        detail::validate_ranks(data, ranks);
        std::vector<typename Container::value_type> answers;
        answers.reserve(ranks.size());
        for (const auto rank : ranks) {
            // Ядро и внешний интерфейс используют ранги с единицы.
            answers.push_back(detail::radix::radix_select(data, rank));
        }
        std::vector<iterator> result;
        result.reserve(ranks.size());
        // Только после ВСЕХ перестановок находим окончательные итераторы.
        // Равные статистики могут ссылаться на один экземпляр: контракт допускает это.
        for (const auto value : answers) {
            result.push_back(std::find(data.begin(), data.end(), value));
        }
        return result;
    }
};

// Дополнительный вариант из локального проекта: полная LSD-сортировка по байтам.
template <typename Container>
class RadixSortAlgorithm : public ExactOrderStatisticAlgorithm<Container> {
public:
    using iterator = typename ExactOrderStatisticAlgorithm<Container>::iterator;

    RadixSortAlgorithm() = default;

    std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: массив и ранги с единицы, пустой список означает отсутствие работы.
         * Возвращает: итераторы begin+(rank-1) после полной сортировки массива.
         */
        detail::validate_ranks(data, ranks);
        if (!ranks.empty()) {
            detail::radix::radix_sort_select(data, 1);
        }
        return detail::rank_iterators(data, ranks);
    }
};
} // namespace rapid_stats

#pragma once

#include "detail/bfprt_impl.hpp"
#include "detail/vector_contract.hpp"

namespace rapid_stats {

template <typename Container> class BfprtAlgorithm : public OrderStatisticAlgorithm<Container> {
public:
    using iterator = typename OrderStatisticAlgorithm<Container>::iterator;

    explicit BfprtAlgorithm(Container& data)
        : OrderStatisticAlgorithm<Container>(data), data_(data) {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: data — изменяемый std::vector чисел, который должен жить дольше объекта.
         * Возвращает: объект BFPRT; контейнер не копируется.
         */
    }

    std::vector<iterator> sort(const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: нулевые ранги, допускаются произвольный порядок и повторы.
         * Возвращает: итераторы на статистики в порядке запроса; старые выбранные позиции сохраняются.
         */
        detail::validate_ranks(data_, ranks);
        auto unique = ranks;
        std::sort(unique.begin(), unique.end());
        unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
        std::size_t left = 0;
        for (const auto rank : unique) {
            // Предыдущие статистики исключены из диапазона и больше не переставляются.
            detail::bfprt::detail::select_range(data_, left, data_.size(), rank);
            left = rank + 1;
        }
        return detail::rank_iterators(data_, ranks);
    }

private:
    Container& data_;
};
} // namespace rapid_stats

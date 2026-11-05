#pragma once

#include "detail/bfprt_impl.hpp"
#include "detail/vector_contract.hpp"

namespace rapid_stats {

template <typename Container>
class BfprtAlgorithm : public ExactOrderStatisticAlgorithm<Container> {
public:
    using iterator = typename ExactOrderStatisticAlgorithm<Container>::iterator;

    BfprtAlgorithm() = default;

    std::vector<iterator> sort(Container& data, const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: массив и ранги с единицы, допускаются произвольный порядок и повторы.
         * Возвращает: итераторы на статистики в порядке запроса; старые выбранные позиции сохраняются.
         */
        detail::validate_ranks(data, ranks);
        auto unique = ranks;
        std::sort(unique.begin(), unique.end());
        unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
        std::size_t left = 0;
        for (const auto rank : unique) {
            // Предыдущие статистики исключены из диапазона и больше не переставляются.
            detail::bfprt::detail::select_range(data, left, data.size(), rank - 1);
            left = rank;
        }
        return detail::rank_iterators(data, ranks);
    }
};
} // namespace rapid_stats

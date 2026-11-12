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
         * Автор: Polikarpov Kirill
         * Принимает: массив и ранги от 1 до n, включая повторы и произвольный порядок.
         * Возвращает: итераторы begin+(rank-1) в порядке запроса после общего MSD-выбора.
         */
        detail::validate_ranks(data, ranks); // Все ошибки проверяются до перестановок.
        auto positions = ranks;
        for (auto& position : positions) {
            --position; // Единственный переход от внешнего ранга к внутреннему индексу.
        }
        std::sort(positions.begin(), positions.end()); // Сортируем запросы, не данные.
        positions.erase(std::unique(positions.begin(), positions.end()), positions.end());
        detail::radix::select_positions(data, positions);
        // Все позиции уже окончательные: нет поиска по значению и повторных проходов.
        return detail::rank_iterators(data, ranks);
    }
};
} // namespace rapid_stats

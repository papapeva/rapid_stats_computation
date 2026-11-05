#pragma once

#include "detail/radix_impl.hpp"
#include "detail/vector_contract.hpp"

#include <algorithm>

namespace rapid_stats {

template <typename Container>
class RadixSelectAlgorithm : public OrderStatisticAlgorithm<Container> {
public:
    using iterator = typename OrderStatisticAlgorithm<Container>::iterator;

    explicit RadixSelectAlgorithm(Container& data)
        : OrderStatisticAlgorithm<Container>(data), data_(data) {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: std::vector<int64_t>, std::vector<uint64_t> или std::vector<double>.
         * Возвращает: объект поразрядного выбора без полной сортировки.
         */
    }

    std::vector<iterator> sort(const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: нулевые ранги; допускаются повторы и произвольный порядок.
         * Возвращает: итераторы на найденные значения в исходном контейнере, не обязательно begin+rank.
         */
        detail::validate_ranks(data_, ranks);
        std::vector<typename Container::value_type> answers;
        answers.reserve(ranks.size());
        for (const auto rank : ranks) {
            // Ядро использует ранг с единицы; внешний интерфейс репозитория — с нуля.
            answers.push_back(detail::radix::radix_select(data_, rank + 1));
        }
        std::vector<iterator> result;
        result.reserve(ranks.size());
        // Только после ВСЕХ перестановок находим окончательные итераторы.
        // Равные статистики могут ссылаться на один экземпляр: контракт допускает это.
        for (const auto value : answers) {
            result.push_back(std::find(data_.begin(), data_.end(), value));
        }
        return result;
    }

private:
    Container& data_;
};

// Дополнительный вариант из локального проекта: полная LSD-сортировка по байтам.
template <typename Container> class RadixSortAlgorithm : public OrderStatisticAlgorithm<Container> {
public:
    using iterator = typename OrderStatisticAlgorithm<Container>::iterator;

    explicit RadixSortAlgorithm(Container& data)
        : OrderStatisticAlgorithm<Container>(data), data_(data) {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: изменяемый std::vector машинных чисел фиксированной ширины.
         * Возвращает: объект полной поразрядной сортировки с интерфейсом выбора статистик.
         */
    }

    std::vector<iterator> sort(const std::vector<std::size_t>& ranks) override {
        /*
         * Автор: Codex (OpenAI).
         * Принимает: нулевые ранги, пустой список означает отсутствие работы.
         * Возвращает: итераторы begin+rank после полной сортировки массива.
         */
        detail::validate_ranks(data_, ranks);
        if (!ranks.empty()) {
            detail::radix::radix_sort_select(data_, 1);
            // LSD меняет хранилище vector обменом с буфером: обновляем границы базового класса.
            this->begin_ = data_.begin();
            this->end_ = data_.end();
        }
        return detail::rank_iterators(data_, ranks);
    }

private:
    Container& data_;
};
} // namespace rapid_stats

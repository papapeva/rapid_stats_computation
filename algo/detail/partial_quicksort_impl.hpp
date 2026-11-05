#pragma once
// Partial Quicksort: отсортировать первые m наименьших значений массива.
// m — КОЛИЧЕСТВО элементов (0..n), k в select_kth — РАНГ (1..n).
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace rapid_stats::detail::partial {
namespace detail {
// Полуоткрытые диапазоны [left,right): правая граница не включена.
template <class T>
std::pair<std::size_t, std::size_t> partition(std::vector<T>& a, std::size_t left,
                                              std::size_t right, T pivot) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: массив, границы и опорное значение.
     * Возвращает: границы блока равных, сохраняя все исходные элементы.
     */
    // pivot — копия: обмен элементов не должен менять опорное значение.
    std::size_t less = left, current = left, greater = right;
    // Инвариант: [left,less)<pivot; [less,current)==pivot;
    // [current,greater) ещё не проверен; [greater,right)>pivot.
    while (current < greater) {
        if (a[current] < pivot) {
            std::swap(a[less], a[current]);
            ++less;
            ++current; // Расширяем блок меньших и проверенную часть.
        } else if (pivot < a[current]) {
            --greater;
            std::swap(a[current], a[greater]);
            // Пришедшее справа число ещё не проверено: current не увеличиваем.
        } else {
            ++current; // Значение равное pivot присоединяется к среднему блоку.
        }
    }
    return {less, greater}; // Границы равных; повторы не требуют рекурсии.
}

template <class T>
void prefix(std::vector<T>& a, std::size_t left, std::size_t right, std::size_t end,
            std::mt19937_64& rng) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: массив, диапазон, абсолютную границу префикса и генератор.
     * Возвращает: ничего; сортирует только части, пересекающие нужный префикс.
     */
    // end — абсолютная граница нужного префикса. Её НЕ пересчитываем в рекурсии.
    while (right - left > 1 && left < end) {
        // Равномерный случайный pivot даёт ожидаемую сложность O(n + m log m).
        std::uniform_int_distribution<std::size_t> choose(left, right - 1);
        const T pivot = a[choose(rng)];
        const auto bounds = partition(a, left, right, pivot);
        const auto less = bounds.first, greater = bounds.second;
        if (greater >= end) {
            // Граница префикса находится слева или внутри равных.
            // Правую часть не трогаем; равные уже стоят правильно.
            right = less;
            continue;
        }
        // Нужны обе строгие части: левая полностью, правая до границы end.
        // Это тот же Quicksort; std::sort внутри алгоритма не используется.
        // Рекурсивно берём МЕНЬШУЮ часть, большую продолжаем циклом.
        // Поэтому стек O(log n) даже при неудачных опорных элементах.
        if (less - left < right - greater) {
            prefix(a, left, less, end, rng);
            left = greater;
        } else {
            prefix(a, greater, right, end, rng);
            right = less;
        }
    }
}
} // namespace detail

template <class T> void validate(const std::vector<T>& a, std::size_t m) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: массив и параметр размера или ранга по контракту ядра.
     * Возвращает: ничего; проверяет аргументы до перестановки элементов.
     */
    static_assert(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, "numeric type required");
    if (m > a.size())
        throw std::out_of_range("m must be in [0,n]");
    if (m == 0)
        return; // Контракт: буквально ничего не делать, даже для пустого массива.
    if constexpr (std::is_floating_point_v<T>) {
        /*
     * Автор: Codex (OpenAI).
     * Принимает: выбранную во время компиляции ветвь проверки числового типа.
     * Возвращает: ничего; этот блок является if constexpr, а не отдельной функцией.
     */
        for (T x : a)
            if (!std::isfinite(x))
                throw std::invalid_argument("finite numbers required");
    }
}

// Изменяет исходный массив по ссылке. Ответ здесь — сам переставленный массив.
// Ожидаемое время O(n + m log m), худшее O(n^2); стек O(log n).
// seed позволяет воспроизводить выбор pivot, на точность результата не влияет.
template <class T>
void partial_quicksort(std::vector<T>& a, std::size_t m, std::uint64_t seed = 20261105) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: массив, количество наименьших элементов m и seed.
     * Возвращает: ничего; первые m минимальных значений становятся отсортированными.
     */
    validate(a, m); // Проверки ДО изменения данных; m=0 — допустимый случай.
    if (m == 0 || a.size() < 2)
        return;
    std::mt19937_64 rng(seed); // Локальный генератор: нет общего изменяемого состояния.
    detail::prefix(a, 0, a.size(), m, rng);
}

// Адаптер к общему контракту точных алгоритмов: массив + ранг 1..n -> число.
template <class T> T select_kth(std::vector<T>& a, std::size_t k, std::uint64_t seed = 20261105) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: изменяемый массив и ранг по контракту внутренней функции.
     * Возвращает: найденное значение; неверные аргументы вызывают исключение.
     */
    if (a.empty())
        throw std::invalid_argument("empty array has no order statistic");
    if (k == 0 || k > a.size())
        throw std::out_of_range("rank k must be in [1,n]");
    partial_quicksort(a, k, seed); // Первые k значений будут отсортированы.
    return a[k - 1]; // Только здесь ранг переводится в индекс с нуля.
}
} // namespace rapid_stats::detail::partial

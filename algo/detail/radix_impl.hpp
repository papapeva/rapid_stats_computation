#pragma once

// Два ТОЧНЫХ алгоритма. Внешний ранг k всегда от 1 до n.
// radix_sort_select: полностью сортирует массив (LSD, от младшего байта).
// radix_select: выбирает только нужную группу (MSD, от старшего байта), как в PDF.
#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace rapid_stats::detail::radix {
static_assert(CHAR_BIT == 8, "8-bit bytes required");
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559 &&
                  std::numeric_limits<double>::radix == 2 &&
                  std::numeric_limits<double>::digits == 53,
              "IEEE 754 binary64 required");
constexpr std::uint64_t sign_bit = std::uint64_t{1} << 63;

// Ключи сравниваются как unsigned, но сохраняют числовой порядок исходного типа.
inline std::uint64_t key(std::uint64_t x) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: машинное число поддерживаемого типа.
     * Возвращает: 64-битный ключ, сохраняющий числовой порядок.
     */
    return x;
}
inline std::uint64_t key(std::int64_t x) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: машинное число поддерживаемого типа.
     * Возвращает: 64-битный ключ, сохраняющий числовой порядок.
     */
    // Преобразование отрицательного числа к unsigned определено стандартом C++17.
    // Инверсия старшего бита переносит отрицательные перед неотрицательными.
    return static_cast<std::uint64_t>(x) ^ sign_bit;
}
inline std::uint64_t key(double x) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: машинное число поддерживаемого типа.
     * Возвращает: 64-битный ключ, сохраняющий числовой порядок.
     */
    if (x == 0.0)
        x = 0.0; // Оба знака нуля получают один ключ; сам массив не меняем.
    std::uint64_t bits = 0;
    std::memcpy(&bits, &x, sizeof(bits)); // Без нарушения строгой алиасности.
    // У отрицательных double порядок битов обратный числовому: инвертируем все биты.
    return (bits & sign_bit) ? ~bits : (bits ^ sign_bit);
}

template <class T> void validate(const std::vector<T>& a, std::size_t k) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: массив и параметр размера или ранга по контракту ядра.
     * Возвращает: ничего; проверяет аргументы до перестановки элементов.
     */
    static_assert(std::is_same_v<T, std::int64_t> || std::is_same_v<T, std::uint64_t> ||
                      std::is_same_v<T, double>,
                  "Supported types: int64_t, uint64_t, double");
    if (a.empty())
        throw std::invalid_argument("array must not be empty");
    if (k == 0 || k > a.size())
        throw std::out_of_range("rank k must be in [1,n]");
    if constexpr (std::is_same_v<T, double>) {
        /*
     * Автор: Codex (OpenAI).
     * Принимает: выбранную во время компиляции ветвь проверки числового типа.
     * Возвращает: ничего; этот блок является if constexpr, а не отдельной функцией.
     */
        // Полный проход ДО перестановок, даже если выбранный ранг можно найти раньше.
        for (double x : a)
            if (!std::isfinite(x))
                throw std::invalid_argument("NaN and infinity are forbidden");
    }
}

template <class T> std::size_t byte(T x, int t) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: число и номер байта t от 0 до 7.
     * Возвращает: значение указанного байта упорядоченного ключа.
     */
    return static_cast<std::size_t>((key(x) >> (8 * t)) & 255U);
}

// Полная стабильная поразрядная сортировка: 8 проходов по 256 корзинам.
// Время O(8*(n+256)), дополнительная память O(n+256).
template <class T> T radix_sort_select(std::vector<T>& a, std::size_t k) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: массив машинных чисел и внутренний ранг k от 1 до n.
     * Возвращает: статистику после полной стабильной поразрядной сортировки.
     */
    validate(a, k);
    std::vector<T> buffer(a.size()); // Второй массив для стабильного распределения.
    for (int t = 0; t < 8; ++t) { // LSD: начинаем с младшего байта.
        std::array<std::size_t, 256> count{}; // size_t вмещает размер вектора.
        for (T x : a)
            ++count[byte(x, t)];
        std::array<std::size_t, 256> next{};
        for (std::size_t d = 1; d < 256; ++d)
            next[d] = next[d - 1] + count[d - 1]; // Начальные позиции корзин.
        // Обход слева направо сохраняет порядок по ранее обработанным младшим байтам.
        for (T x : a)
            buffer[next[byte(x, t)]++] = x;
        a.swap(buffer); // Обмениваем хранилища без копирования всех элементов.
    }
    return a[k - 1]; // Единственное преобразование внешнего ранга в индекс.
}

// Перенос выбранной корзины в начало [lo,hi); все значения сохраняются обменами.
template <class T>
std::size_t compact_bucket(std::vector<T>& a, std::size_t lo, std::size_t hi, int t,
                           std::size_t chosen) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: массив, диапазон, номер байта и выбранную корзину.
     * Возвращает: новую правую границу выбранной группы после переноса обменами.
     */
    std::size_t write = lo;
    for (std::size_t i = lo; i < hi; ++i) {
        if (byte(a[i], t) == chosen) {
            std::swap(a[write], a[i]);
            ++write;
        }
    }
    return write;
}

// Именно Radix Select из PDF: не сортирует массив и не обещает ответ на позиции k-1.
// Возвращает значение; O(8*n), O(256) памяти, максимум два прохода на байт.
template <class T> T radix_select(std::vector<T>& a, std::size_t k) {
    /*
     * Автор: Codex (OpenAI).
     * Принимает: массив машинных чисел и внутренний ранг k от 1 до n.
     * Возвращает: точное значение без полной сортировки массива.
     */
    validate(a, k);
    const std::size_t lo = 0;
    std::size_t hi = a.size();
    std::size_t r = k; // Локальный ранг в текущем диапазоне, тоже с единицы.
    for (int t = 7; t >= 0; --t) { // MSD: старшие байты определяют порядок раньше младших.
        if (r == 0 || r > hi - lo)
            throw std::logic_error("rank invariant broken");
        if (hi - lo == 1)
            return a[lo];
        std::array<std::size_t, 256> count{};
        for (std::size_t i = lo; i < hi; ++i)
            ++count[byte(a[i], t)];
        if (r == 0 || r > hi - lo)
            throw std::logic_error("rank invariant after counting");
        std::size_t chosen = 0;
        while (chosen < 256 && r > count[chosen]) {
            r -= count[chosen]; // Пропускаем все меньшие корзины, включая пустые.
            ++chosen;
        }
        if (chosen == 256 || count[chosen] == 0 || r == 0 || r > count[chosen])
            throw std::logic_error("bucket invariant broken");
        hi = compact_bucket(a, lo, hi, t, chosen);
        if (hi - lo != count[chosen])
            throw std::logic_error("compaction invariant broken");
    }
    return a[lo]; // После восьми байтов все оставшиеся ключи равны.
}
} // namespace rapid_stats::detail::radix

#pragma once

// Совместный точный выбор рангов по байтам MSD без полной сортировки.
// Внутренние позиции нулевые, публичные ранги начинаются с единицы.
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
     * Автор: Polikarpov Kirill
     * Принимает: машинное число поддерживаемого типа.
     * Возвращает: 64-битный ключ, сохраняющий числовой порядок.
     */
    return x;
}
inline std::uint64_t key(std::int64_t x) {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: машинное число поддерживаемого типа.
     * Возвращает: 64-битный ключ, сохраняющий числовой порядок.
     */
    // Преобразование отрицательного числа к unsigned определено стандартом C++17.
    // Инверсия старшего бита переносит отрицательные перед неотрицательными.
    return static_cast<std::uint64_t>(x) ^ sign_bit;
}
inline std::uint64_t key(double x) {
    /*
     * Автор: Polikarpov Kirill
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

// Внутренняя статистика работы для тестов: не меняет результат алгоритма.
struct WorkStats {
    std::array<std::size_t, 8> counted{};
    std::array<std::size_t, 8> distributed{};
    std::array<std::size_t, 8> nodes{};
};

template <class T>
void select_ranges(std::vector<T>& data, const std::vector<std::size_t>& positions,
                   std::size_t first, std::size_t last, std::size_t lo, std::size_t hi,
                   int byte_index, WorkStats* stats) {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: массив, позиции [first,last), диапазон [lo,hi), байт и счётчики работы.
     * Возвращает: ничего; устанавливает запрошенные статистики на их позициях.
     */
    if (first == last || hi - lo <= 1 || byte_index < 0) {
        return;
    }
    if (positions[first] < lo || positions[last - 1] >= hi) {
        throw std::logic_error("requested positions outside radix range");
    }
    const auto level = static_cast<std::size_t>(byte_index);
    if (stats != nullptr) {
        stats->counted[level] += hi - lo;
        ++stats->nodes[level];
    }
    // Первый проход: общая гистограмма для ВСЕХ рангов текущего диапазона.
    std::array<std::size_t, 256> next{};
    const auto first_key = key(data[lo]);
    std::uint64_t differing_bits = 0;
    for (std::size_t i = lo; i < hi; ++i) {
        const auto current_key = key(data[i]);
        differing_bits |= current_key ^ first_key;
        ++next[(current_key >> (8 * byte_index)) & 255U];
    }
    if (differing_bits == 0) {
        return; // Все ключи равны: каждая позиция диапазона уже подходит.
    }
    // Границы корзин абсолютные: сохраняют глобальные позиции статистик.
    std::array<std::size_t, 257> bounds{};
    bounds[0] = lo;
    for (std::size_t d = 0; d < 256; ++d) {
        bounds[d + 1] = bounds[d] + next[d];
        next[d] = bounds[d]; // Счётчик переиспользуется как курсор свободного места.
    }
    // Второй проход (циклами обменов): помещаем элементы в корзины на месте.
    // Каждый шаг увеличивает один курсор; всего не более hi-lo шагов.
    // Внутри корзин значения ещё НЕ отсортированы по младшим байтам.
    for (std::size_t d = 0; d < 256; ++d) {
        while (next[d] < bounds[d + 1]) {
            const auto destination =
                static_cast<std::size_t>((key(data[next[d]]) >> (8 * byte_index)) & 255U);
            if (stats != nullptr) {
                ++stats->distributed[level];
            }
            if (destination == d) {
                ++next[d];
            } else {
                if (next[destination] >= bounds[destination + 1]) {
                    throw std::logic_error("radix destination bucket is full");
                }
                std::swap(data[next[d]], data[next[destination]]);
                ++next[destination];
            }
        }
    }
    // Распределяем запросы по тем же границам. Пустые по запросам корзины
    // больше не трогаем. Разные ветви работают с непересекающимися диапазонами.
    auto request = first;
    for (std::size_t d = 0; d < 256 && request < last; ++d) {
        const auto begin_request = request;
        while (request < last && positions[request] < bounds[d + 1]) {
            ++request;
        }
        if (begin_request != request) {
            select_ranges(data, positions, begin_request, request, bounds[d], bounds[d + 1],
                          byte_index - 1, stats);
        }
    }
}

template <class T>
void select_positions(std::vector<T>& data, const std::vector<std::size_t>& positions,
                      WorkStats* stats = nullptr) {
    /*
     * Автор: Polikarpov Kirill
     * Принимает: проверенный массив, отсортированные уникальные позиции и диагностику.
     * Возвращает: ничего; после перестановок data[position] содержит статистику.
     */
    static_assert(std::is_same_v<T, std::int64_t> || std::is_same_v<T, std::uint64_t> ||
                      std::is_same_v<T, double>,
                  "Supported types: int64_t, uint64_t, double");
    select_ranges(data, positions, 0, positions.size(), 0, data.size(), 7, stats);
}
} // namespace rapid_stats::detail::radix

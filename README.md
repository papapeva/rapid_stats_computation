# Быстрое вычисление порядковых статистик

Учебный проект на C++17: k-я порядковая статистика на больших выборках и сравнение алгоритмов отбора.

Порядковая статистика ранга `k` — элемент, который стоял бы на позиции `k`, если упорядочить выборку. Ранг нулевой: при компараторе по умолчанию `k = 0` — минимум, `k = n - 1` — максимум. Нижняя медиана — ранг `(n - 1) / 2`.

Библиотека header-only: алгоритмы ш аблонные и лежат в `include/`. Каталога `src/` пока нет. Нешаблонный код позже стоит вынести в `src/` и сменить цель `rapid_stats` с `INTERFACE` на обычную библиотеку.

## Требования

- CMake 3.20 или новее
- компилятор C++17: GCC, Clang или MSVC
- для форматирования: `clang-format` 18 (`pip install clang-format==18.1.8`)
- для статического анализа: `clang-tidy` и генератор Ninja (нужен `compile_commands.json`)

Внешних библиотек нет: тесты подключаются к CTest своим небольшим прогоном, конфигурация не ходит в сеть.

## Структура

```text
include/rapid_stats/          публичный API
include/rapid_stats/detail/   реализации алгоритмов
tests/                        автотесты (CTest)
benchmarks/                   сравнение времени и числа сравнений
examples/                     короткий пример вызова
cmake/                        предупреждения компилятора и цели линтеров
.github/workflows/ci.yml      сборка, тесты, clang-format, clang-tidy
```

Публичная точка входа — `rapid_stats/order_statistic.hpp`, цель CMake — `rapid_stats::rapid_stats`.

## Сборка и тесты

Один каталог сборки на тип конфигурации. Пресеты: `debug` и `release`.

```text
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Без пресетов, генератор Ninja или Makefiles:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Visual Studio — мультиконфигурационный генератор, тип сборки задаётся на шаге build:

```text
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Исполняемые файлы попадают в `build/bin/` (или `build-release/bin/`):

- `rapid_stats_tests` — автотесты
- `rapid_stats_example` — пример
- `rapid_stats_bench` — бенчмарк

## Бенчмарк

Запускать в Release: в Debug цифры времени не показательны.

```text
cmake --preset release
cmake --build --preset release --target rapid_stats_bench
build-release/bin/rapid_stats_bench 1000000
```

Аргумент — размер выборки от 2 до 50 000 000, по умолчанию 1 000 000. В таблице три свои алгоритма и `std::nth_element` как библиотечный ориентир. Колонка `ms` включает копирование выборки (API его не портит). Колонка `comparisons` считает только вызовы компаратора, отдельным прогоном.

## Линтеры

После конфигурации:

```text
cmake --build build --target format
cmake --build build --target format-check
cmake --build build --target tidy
```

`format` переписывает исходники. `format-check` только проверяет стиль и падает при расхождении. `tidy` гоняет clang-tidy по `.cpp`; заголовки библиотеки он видит через них. Для `tidy` нужен Ninja или Makefile: у генератора Visual Studio нет `compile_commands.json`.

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --target tidy
```

В CI включены предупреждения как ошибки, AddressSanitizer и UBSan.

## Алгоритмы

Все три метода читают копию выборки. Сложность ниже — это уже фаза отбора, без учёта общего копирования.

| Алгоритм | Имя в коде | Время | Зачем в сравнении |
| --- | --- | --- | --- |
| Полная сортировка | `FullSort` | O(n log n) | Эталон правильности |
| Quickselect, pivot — медиана трёх | `Quickselect` | среднее O(n), худшее O(n²) | Практический линейный отбор, в том числе медиана |
| Куча размера k + 1 | `HeapSelect` | O(n log k) | Маленький ранг на большой выборке |

`order_statistic` по умолчанию вызывает quickselect. Компаратор задаёт порядок так же, как в `std::sort`: `std::greater<>` делает `k = 0` максимумом.

## Как добавить алгоритм

1. Новое значение в `enum class Algorithm`.
2. Реализация в `include/rapid_stats/detail/` с тем же контрактом, что у `std::nth_element`: после вызова в позиции `nth` лежит нужный элемент.
3. Ветка в `order_statistic` и имя в `algorithm_name`. Флаг `-Wswitch-enum` (и MSVC 4062) напомнит о пропущенной ветке.
4. Элемент в `kAlgorithms`, чтобы тесты и бенчмарк подхватили метод сами.
5. При новом `.cpp` в тестах — строка в `tests/CMakeLists.txt`. Новый сценарий в существующем файле достаточно оформить макросом `RS_TEST`.

Ближайшее продолжение по теме: introselect или медиана медиан (линейное время в худшем случае), выбор метода по отношению `k / n`, потоковые и внешние квантили, когда выборка не помещается в память.

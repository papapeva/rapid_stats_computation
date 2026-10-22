# Быстрое вычисление порядковых статистик

Учебный проект на C++17: поиск порядковых статистик.

Порядковая статистика ранга `k` — элемент, который стоял бы на позиции `k`, если упорядочить выборку. Ранг нулевой: `k = 0` — минимум, `k = n - 1` — максимум.

Код алгоритмов — каталог `algo/`. Базовый класс — `algo/order_statistic_algorithm.hpp`. Любой алгоритм наследуется от `rapid_stats::OrderStatisticAlgorithm` и переопределяет `sort`. Сборка — `CMakeLists.txt`.

```text
algo/order_statistic_algorithm.hpp  абстрактный класс
check_function_comments.py          комментарий в теле функции
CMakeLists.txt                      сборка
AGENTS.md                           правила для агентов
README.md                           эта инструкция
.clang-format                       стиль кода
.clang-tidy                         статический анализ
.github/workflows/ci.yml            проверка pull request на GitHub
```

## Клонирование

```text
git clone git@github.com:papapeva/rapid_stats_computation.git
cd rapid_stats_computation
```

## Ветки

В `master` напрямую не пушат: изменение попадает туда только через pull request и ревью.

Имя ветки строго такое: `<номер задачи>/<краткое название>`. Номер задачи — код из трекера, название короткое, латиницей, слова через `_`. Пример: `QUI-1/structure_was_built`.

```text
git checkout master
git pull
git checkout -b QUI-1/structure_was_built
```

Дальше коммиты делаются в этой ветке. Когда работа готова:

```text
git push -u origin QUI-1/structure_was_built
```

На GitHub открой pull request в `master`. Заголовок pull request строго начинается с номера задачи: `QUI-1 | example_title`. CI отклонит другой формат. После замечаний дописывай коммиты в ту же ветку и снова пушь её.

## Требования

- CMake 3.20 или новее
- компилятор C++17
- `clang-format` 18 и `clang-tidy` для линтеров

Внешних библиотек нет.

## Сборка

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Линтеры

```text
cmake --build build --target format
cmake --build build --target format-check
cmake --build build --target tidy
```

`format` переписывает исходники. `format-check` только проверяет стиль. `tidy` гоняет clang-tidy и проверяет комментарий в теле каждой функции из `algo/order_statistic_algorithm.hpp`.

Первая строка внутри функции — многострочный комментарий. Сначала автор этой функции, затем что она принимает и что возвращает:

```cpp
void example(int value) {
    /*
     * Автор: Имя Фамилия
     * Принимает: value — смысл аргумента.
     * Возвращает: смысл результата.
     */
}
```

У объявления без тела (`= default`, `= delete`, `= 0`) такой строки нет, комментарий там не требуется.

`clang-format` должен быть версии 18, иначе проверка стиля разойдётся с CI:

```text
pip install clang-format==18.1.8
```

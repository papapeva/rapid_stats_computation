# Быстрое вычисление порядковых статистик

Учебный проект на C++17: k-я порядковая статистика на больших выборках.

Порядковая статистика ранга `k` — элемент, который стоял бы на позиции `k`, если упорядочить выборку. Ранг нулевой: `k = 0` — минимум, `k = n - 1` — максимум.

Весь код алгоритма — `order_statistic.hpp`. Тесты — `order_statistic_test.cpp`. Сборка — `CMakeLists.txt`. Сейчас в заголовке два метода: полная сортировка и quickselect. Новый алгоритм добавляется в `enum class Algorithm`.

```text
order_statistic.hpp       алгоритм
order_statistic_test.cpp  тесты
CMakeLists.txt            сборка
AGENTS.md                 правила для агентов
README.md                 эта инструкция
.clang-format             стиль кода
.clang-tidy               статический анализ
.github/workflows/ci.yml  проверка pull request на GitHub
```

## Клонирование

```text
git clone git@github.com:papapeva/rapid_stats_computation.git
cd rapid_stats_computation
```

## Ветки

В `master` напрямую не пушат: изменение попадает туда только через pull request и ревью.

```text
git checkout master
git pull
git checkout -b short-description
```

Дальше коммиты делаются в этой ветке. Когда работа готова:

```text
git push -u origin short-description
```

На GitHub открой pull request в `master`. После замечаний дописывай коммиты в ту же ветку и снова пушь её.

## Требования

- CMake 3.20 или новее
- компилятор C++17
- `clang-format` 18 и `clang-tidy` для линтеров

Внешних библиотек нет.

## Сборка и тесты

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Линтеры

```text
cmake --build build --target format
cmake --build build --target format-check
cmake --build build --target tidy
```

`format` переписывает исходники. `format-check` только проверяет стиль. `tidy` нужен каталог сборки Ninja: у него есть `compile_commands.json`.

`clang-format` должен быть версии 18, иначе проверка стиля разойдётся с CI:

```text
pip install clang-format==18.1.8
```

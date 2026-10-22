# Проверяет, что первая строка тела каждой функции — многострочный комментарий
# с автором этой функции и описанием аргументов и результата.

import re
import sys
from dataclasses import dataclass

SKIP_NAMES = {
    "if",
    "for",
    "while",
    "switch",
    "catch",
    "sizeof",
    "alignof",
    "decltype",
    "typeid",
    "static_assert",
    "noexcept",
    "return",
    "co_return",
}
CAST_NAMES = {
    "static_cast",
    "dynamic_cast",
    "const_cast",
    "reinterpret_cast",
}
DENIED_BEFORE_NAME = {",", ":", ".", "->", "=", "?", "(", ")", "[", "]", "{", "}"}
LAMBDA_BEFORE = {"=", "(", ",", ";", "{", "}", "return", "throw", "?", ":", "&&", "||", "co_return"}
SUFFIX_WORDS = {"const", "volatile", "override", "final", "mutable", "constexpr"}
MULTI_CHAR_OPS = (
    "<<=",
    ">>=",
    "->*",
    "...",
    "<<",
    ">>",
    "==",
    "!=",
    "<=",
    ">=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "&=",
    "|=",
    "^=",
    "&&",
    "||",
    "++",
    "--",
    "->",
    "::",
    ".*",
)
FIELD_AUTHOR = "Автор"
FIELD_ACCEPTS = "Принимает"
FIELD_RETURNS = "Возвращает"
EXPECTED = """\
    /*
     * Автор: имя автора этой функции
     * Принимает: что функция принимает
     * Возвращает: что функция возвращает
     */"""


@dataclass
class Token:
    kind: str
    text: str
    line: int
    start: int
    end: int


def lex(source):
    tokens = []
    index = 0
    line = 1
    length = len(source)
    while index < length:
        char = source[index]
        if char == "\n":
            line += 1
            index += 1
            continue
        if char in " \t\r\v\f":
            index += 1
            continue
        if char == "#" and _at_line_start(source, index):
            index = _skip_preprocessor(source, index)
            continue
        if char == "/" and index + 1 < length and source[index + 1] == "/":
            index = source.find("\n", index)
            if index == -1:
                break
            continue
        if char == "/" and index + 1 < length and source[index + 1] == "*":
            end = source.find("*/", index + 2)
            if end == -1:
                break
            line += source[index:end + 2].count("\n")
            index = end + 2
            continue
        if char == '"':
            end = _skip_string(source, index)
            tokens.append(Token("str", source[index:end], line, index, end))
            line += source[index:end].count("\n")
            index = end
            continue
        if char == "'":
            end = _skip_char(source, index)
            tokens.append(Token("str", source[index:end], line, index, end))
            index = end
            continue
        if char == "R" and index + 1 < length and source[index + 1] == '"':
            end = _skip_raw_string(source, index)
            tokens.append(Token("str", source[index:end], line, index, end))
            line += source[index:end].count("\n")
            index = end
            continue
        if char.isalpha() or char == "_":
            end = index + 1
            while end < length and (source[end].isalnum() or source[end] == "_"):
                end += 1
            tokens.append(Token("id", source[index:end], line, index, end))
            index = end
            continue
        if char.isdigit():
            end = index + 1
            while end < length and (source[end].isalnum() or source[end] in "._"):
                end += 1
            tokens.append(Token("num", source[index:end], line, index, end))
            index = end
            continue
        for op in MULTI_CHAR_OPS:
            if source.startswith(op, index):
                tokens.append(Token("op", op, line, index, index + len(op)))
                index += len(op)
                break
        else:
            tokens.append(Token("op", char, line, index, index + 1))
            index += 1
    return tokens


def _at_line_start(source, index):
    while index > 0 and source[index - 1] in " \t":
        index -= 1
    return index == 0 or source[index - 1] == "\n"


def _skip_preprocessor(source, index):
    while index < len(source):
        if source[index] == "\n":
            if index > 0 and source[index - 1] == "\\":
                index += 1
                continue
            return index
        index += 1
    return index


def _skip_string(source, index):
    index += 1
    while index < len(source):
        if source[index] == "\\":
            index += 2
            continue
        if source[index] == '"':
            return index + 1
        index += 1
    return index


def _skip_char(source, index):
    index += 1
    while index < len(source):
        if source[index] == "\\":
            index += 2
            continue
        if source[index] == "'":
            return index + 1
        index += 1
    return index


def _skip_raw_string(source, index):
    index += 2
    delim_end = source.find("(", index)
    if delim_end == -1:
        return len(source)
    delimiter = source[index:delim_end]
    closer = ")" + delimiter + '"'
    end = source.find(closer, delim_end + 1)
    if end == -1:
        return len(source)
    return end + len(closer)


def _match_forward(tokens, open_index, open_text, close_text):
    depth = 0
    for index in range(open_index, len(tokens)):
        if tokens[index].text == open_text:
            depth += 1
        elif tokens[index].text == close_text:
            depth -= 1
            if depth == 0:
                return index
    return None


def _match_backward(tokens, close_index, open_text, close_text):
    depth = 0
    for index in range(close_index, -1, -1):
        if tokens[index].text == close_text:
            depth += 1
        elif tokens[index].text == open_text:
            depth -= 1
            if depth == 0:
                return index
    return None


def _predecessor_ok(tokens, name_index):
    if name_index <= 0:
        return True
    return tokens[name_index - 1].text not in DENIED_BEFORE_NAME


def _is_plain_function_paren(tokens, index):
    if index == 0 or tokens[index - 1].kind != "id":
        return False
    name = tokens[index - 1].text
    if name in SKIP_NAMES or name in CAST_NAMES:
        return False
    return _predecessor_ok(tokens, index - 1)


def _is_template_function_paren(tokens, index):
    if index == 0 or tokens[index - 1].text != ">":
        return False
    angle = _match_backward(tokens, index - 1, "<", ">")
    if angle is None or angle == 0 or tokens[angle - 1].kind != "id":
        return False
    name = tokens[angle - 1].text
    if name in SKIP_NAMES or name in CAST_NAMES:
        return False
    return _predecessor_ok(tokens, angle - 1)


def _is_operator_call_paren(tokens, index):
    if index < 3 or tokens[index - 1].text != ")":
        return False
    open_empty = _match_backward(tokens, index - 1, "(", ")")
    if open_empty is None or open_empty + 1 != index - 1:
        return False
    if open_empty == 0 or tokens[open_empty - 1].text != "operator":
        return False
    return _predecessor_ok(tokens, open_empty - 1)


def _is_operator_index_paren(tokens, index):
    if index < 3 or tokens[index - 1].text != "]":
        return False
    open_empty = _match_backward(tokens, index - 1, "[", "]")
    if open_empty is None or open_empty + 1 != index - 1:
        return False
    if open_empty == 0 or tokens[open_empty - 1].text != "operator":
        return False
    return _predecessor_ok(tokens, open_empty - 1)


def _is_operator_symbol_paren(tokens, index):
    if index < 2 or tokens[index - 1].kind == "id":
        return False
    if tokens[index - 1].text in {"(", ")", "{", "}", ";", ","}:
        return False
    if tokens[index - 2].text != "operator":
        return False
    return _predecessor_ok(tokens, index - 2)


def _is_lambda_introducer(tokens, bracket_index):
    if bracket_index == 0:
        return True
    return tokens[bracket_index - 1].text in LAMBDA_BEFORE


def _is_lambda_param_paren(tokens, index):
    if index == 0 or tokens[index - 1].text != "]":
        return False
    if _is_operator_index_paren(tokens, index):
        return False
    bracket = _match_backward(tokens, index - 1, "[", "]")
    if bracket is None:
        return False
    return _is_lambda_introducer(tokens, bracket)


def _function_name(tokens, index):
    if _is_lambda_param_paren(tokens, index):
        return "лямбда"
    if tokens[index - 1].kind == "id":
        return tokens[index - 1].text
    if (
        _is_operator_call_paren(tokens, index)
        or _is_operator_index_paren(tokens, index)
        or _is_operator_symbol_paren(tokens, index)
    ):
        return "operator"
    if _is_template_function_paren(tokens, index):
        angle = _match_backward(tokens, index - 1, "<", ">")
        return tokens[angle - 1].text
    return "функция"


def _is_function_paren(tokens, index):
    return (
        _is_plain_function_paren(tokens, index)
        or _is_template_function_paren(tokens, index)
        or _is_operator_call_paren(tokens, index)
        or _is_operator_index_paren(tokens, index)
        or _is_operator_symbol_paren(tokens, index)
        or _is_lambda_param_paren(tokens, index)
    )


def _skip_angles(tokens, index):
    depth = 0
    while index < len(tokens):
        if tokens[index].text == "<":
            depth += 1
        elif tokens[index].text == ">":
            depth -= 1
            if depth == 0:
                return index + 1
        index += 1
    return None


def _skip_trailing_type(tokens, index):
    angle = paren = bracket = 0
    while index < len(tokens):
        text = tokens[index].text
        if text == "<":
            angle += 1
        elif text == ">" and angle:
            angle -= 1
        elif text == "(":
            paren += 1
        elif text == ")" and paren:
            paren -= 1
        elif text == "[":
            bracket += 1
        elif text == "]" and bracket:
            bracket -= 1
        elif angle == paren == bracket == 0 and text in {"{", ";", "override", "final"}:
            return index
        index += 1
    return index


def _skip_ctor_init(tokens, index):
    count = len(tokens)
    if index >= count:
        return None
    while index < count:
        if tokens[index].text == "...":
            index += 1
        while index < count and (
            tokens[index].kind == "id" or tokens[index].text in {"::", "~", "template", "typename"}
        ):
            index += 1
            if index < count and tokens[index].text == "<":
                index = _skip_angles(tokens, index)
                if index is None:
                    return None
        if index < count and tokens[index].text == "(":
            close = _match_forward(tokens, index, "(", ")")
            if close is None:
                return None
            index = close + 1
        elif index < count and tokens[index].text == "{":
            close = _match_forward(tokens, index, "{", "}")
            if close is None:
                return None
            index = close + 1
        else:
            return None
        if index < count and tokens[index].text == ",":
            index += 1
            continue
        return index
    return None


def _body_brace(tokens, after_params):
    index = after_params
    while index < len(tokens):
        text = tokens[index].text
        if text in SUFFIX_WORDS or text in {"&", "&&"}:
            index += 1
            continue
        if text in {"noexcept", "throw"}:
            index += 1
            if index < len(tokens) and tokens[index].text == "(":
                close = _match_forward(tokens, index, "(", ")")
                if close is None:
                    return None
                index = close + 1
            continue
        if text == "->":
            index = _skip_trailing_type(tokens, index + 1)
            continue
        if text == ":":
            nxt = _skip_ctor_init(tokens, index + 1)
            if nxt is None or nxt <= index:
                return None
            index = nxt
            continue
        if text == "try":
            if index + 1 < len(tokens) and tokens[index + 1].text == "{":
                return tokens[index + 1]
            return None
        if text == "{":
            return tokens[index]
        return None
    return None


def function_bodies(tokens):
    bodies = []
    seen = set()
    for index, token in enumerate(tokens):
        if token.text != "(" or not _is_function_paren(tokens, index):
            continue
        close = _match_forward(tokens, index, "(", ")")
        if close is None:
            continue
        brace = _body_brace(tokens, close + 1)
        if brace is None or brace.start in seen:
            continue
        seen.add(brace.start)
        bodies.append((brace, _function_name(tokens, index)))

    for index, token in enumerate(tokens):
        if token.text != "{" or token.start in seen or index == 0 or tokens[index - 1].text != "]":
            continue
        bracket = _match_backward(tokens, index - 1, "[", "]")
        if bracket is None or not _is_lambda_introducer(tokens, bracket):
            continue
        if bracket > 0 and tokens[bracket - 1].text == "operator":
            continue
        seen.add(token.start)
        bodies.append((token, "лямбда"))
    return bodies


def _comment_lines(comment):
    inner = comment[2:]
    if inner.endswith("*/"):
        inner = inner[:-2]
    lines = []
    for line in inner.splitlines():
        text = line.strip()
        if text.startswith("*"):
            text = text[1:].strip()
        if text:
            lines.append(text)
    return lines


def _field_ok(line, label):
    match = re.match(rf"^{label}\s*:\s*(.+)$", line)
    if not match:
        return False
    value = match.group(1).strip()
    return bool(re.search(r"[0-9A-Za-zА-Яа-яЁё]", value))


def check_body(source, path, brace, name):
    line_end = source.find("\n", brace.start)
    if line_end == -1:
        line_end = len(source)
    if source[brace.end:line_end].strip() != "":
        return (
            f"{path}:{brace.line}: первая строка тела «{name}» должна быть комментарием, "
            f"а не кодом на строке со скобкой.\n{EXPECTED}"
        )
    if line_end >= len(source):
        return f"{path}:{brace.line}: в теле «{name}» нет комментария.\n{EXPECTED}"
    next_start = line_end + 1
    next_end = source.find("\n", next_start)
    if next_end == -1:
        next_end = len(source)
    first_line = source[next_start:next_end]
    if first_line.strip() == "" or not first_line.lstrip().startswith("/*"):
        return (
            f"{path}:{brace.line + 1}: первая строка тела «{name}» должна быть "
            f"многострочным комментарием.\n{EXPECTED}"
        )
    comment_at = next_start + (len(first_line) - len(first_line.lstrip()))
    comment_end = source.find("*/", comment_at + 2)
    if comment_end == -1:
        return f"{path}:{brace.line + 1}: комментарий в теле «{name}» не закрыт."
    comment = source[comment_at:comment_end + 2]
    if "\n" not in comment:
        return (
            f"{path}:{brace.line + 1}: комментарий в теле «{name}» должен быть "
            f"многострочным.\n{EXPECTED}"
        )
    lines = _comment_lines(comment)
    checks = (
        (0, FIELD_AUTHOR, "автора этой функции"),
        (1, FIELD_ACCEPTS, "что функция принимает"),
        (2, FIELD_RETURNS, "что функция возвращает"),
    )
    for position, label, meaning in checks:
        if position >= len(lines) or not _field_ok(lines[position], label):
            return (
                f"{path}:{brace.line + 1}: в комментарии «{name}» строка {position + 1} "
                f"должна начинаться с «{label}:» и сообщать {meaning}.\n{EXPECTED}"
            )
    return None


def check_source(path, source):
    errors = []
    for brace, name in function_bodies(lex(source)):
        error = check_body(source, path, brace, name)
        if error:
            errors.append(error)
    return errors


def check_file(path):
    with open(path, encoding="utf-8-sig") as handle:
        return check_source(path, handle.read())


def _self_test():
    valid = """\
void add(int value) {
    /*
     * Автор: Анна Иванова
     * Принимает: value — слагаемое.
     * Возвращает: ничего.
     */
}
"""
    samples = {
        "valid": (valid, 0),
        "missing": ("void add(int value) {\n    return value;\n}\n", 1),
        "same line": ("void add(int value) { /*\n     * Автор: Анна\n     * Принимает: value.\n     * Возвращает: ничего.\n     */\n}\n", 1),
        "blank line": ("void add(int value) {\n\n    /*\n     * Автор: Анна\n     * Принимает: value.\n     * Возвращает: ничего.\n     */\n}\n", 1),
        "line comment": ("void add(int value) {\n    // Автор: Анна\n}\n", 1),
        "one line block": ("void add(int value) {\n    /* Автор: Анна Принимает: value Возвращает: ничего */\n}\n", 1),
        "no author": ("void add(int value) {\n    /*\n     * Принимает: value.\n     * Возвращает: ничего.\n     */\n}\n", 1),
        "author not first": ("void add(int value) {\n    /*\n     * Описание.\n     * Автор: Анна\n     * Принимает: value.\n     * Возвращает: ничего.\n     */\n}\n", 1),
        "controls": (
            "void add(int value) {\n"
            "    /*\n     * Автор: Анна\n     * Принимает: value.\n     * Возвращает: ничего.\n     */\n"
            "    if (value) { return; }\n"
            "    for (;;) { break; }\n"
            "    while (value) { --value; }\n"
            "    switch (value) { default: break; }\n"
            "    try { throw 1; } catch (int error) { (void)error; }\n"
            "}\n",
            0,
        ),
        "defaulted": ("struct Foo { void bar() = default; virtual void baz() = 0; Foo() = delete; };\n", 0),
        "ctor": (
            "struct Foo {\n"
            "    explicit Foo(int value) : a_(value), b_{value} {\n"
            "        /*\n"
            "         * Автор: Анна Иванова\n"
            "         * Принимает: value — начальное значение.\n"
            "         * Возвращает: ничего.\n"
            "         */\n"
            "    }\n"
            "    int a_;\n"
            "    int b_;\n"
            "};\n",
            0,
        ),
        "ctor missing": (
            "struct Foo {\n"
            "    explicit Foo(int value) : end_(std::end(value)) {}\n"
            "};\n",
            1,
        ),
        "lambda": (
            "auto with_args = [](int value) {\n"
            "    /*\n     * Автор: Анна\n     * Принимает: value.\n     * Возвращает: value.\n     */\n"
            "    return value;\n"
            "};\n"
            "auto bare = [with_args] {\n"
            "    /*\n     * Автор: Анна\n     * Принимает: захваченный with_args.\n     * Возвращает: ничего.\n     */\n"
            "};\n",
            0,
        ),
        "lambda missing": ("auto bare = [] { return 1; };\n", 1),
        "trailing": (
            "auto add(int value) const noexcept -> int {\n"
            "    /*\n     * Автор: Анна\n     * Принимает: value.\n     * Возвращает: value.\n     */\n"
            "    return value;\n"
            "}\n",
            0,
        ),
        "operator": (
            "struct Foo {\n"
            "    int operator()(int value) {\n"
            "        /*\n         * Автор: Анна\n         * Принимает: value.\n         * Возвращает: value.\n         */\n"
            "        return value;\n"
            "    }\n"
            "    int operator[](int value) { return value; }\n"
            "    bool operator<(int value) const { return false; }\n"
            "};\n",
            2,
        ),
        "strings": (
            "void add(int value) {\n"
            "    /*\n     * Автор: Анна\n     * Принимает: value.\n     * Возвращает: ничего.\n     */\n"
            '    const char* text = "void hidden() { return; }";\n'
            "    const char* note = R\"( /* not a comment */ )\";\n"
            "}\n",
            0,
        ),
        "allman": (
            "void add(int value)\n"
            "{\n"
            "    /*\n"
            "     * Автор: Анна\n"
            "     * Принимает: value.\n"
            "     * Возвращает: ничего.\n"
            "     */\n"
            "}\n",
            0,
        ),
    }
    failed = False
    for title, (source, expected) in samples.items():
        errors = check_source(title, source)
        if len(errors) != expected:
            failed = True
            print(f"FAIL {title}: expected {expected}, got {len(errors)}")
            for error in errors:
                print(error)
    if failed:
        return 1
    print(f"self-test passed ({len(samples)} cases)")
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == "--self-test":
        return _self_test()
    if len(argv) < 2:
        print("usage: check_function_comments.py <file>...", file=sys.stderr)
        return 2
    errors = []
    for path in argv[1:]:
        errors.extend(check_file(path))
    for error in errors:
        print(error, file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

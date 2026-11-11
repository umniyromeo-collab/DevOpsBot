#!/usr/bin/env bash
# PostToolUse-хук: после того как Claude правит C/C++-файл, прогоняет clang-format
# только по изменённым строкам (git clang-format) — так же, как VS Code с
# "editor.formatOnSaveMode": "modifications". Старый код не по стилю не трогаем,
# чтобы не раздувать diff и не портить blame.
# Новый, ещё не добавленный в git файл форматируется целиком.
# Правку хук не блокирует никогда: при любой проблеме молча выходит с кодом 0.

command -v jq >/dev/null 2>&1 || exit 0

file=$(jq -r '.tool_input.file_path // empty' 2>/dev/null)
[ -n "$file" ] && [ -f "$file" ] || exit 0

case "$file" in
    *.cpp | *.cc | *.cxx | *.h | *.hpp | *.hxx | *.ipp | *.tpp) ;;
    *) exit 0 ;;
esac

project=${CLAUDE_PROJECT_DIR:-$PWD}
case "$file" in
    /*) ;;
    *) file="$PWD/$file" ;;
esac

# Только файлы проекта; сборочные каталоги и скачанные зависимости пропускаем.
case "$file" in
    "$project"/cmake-build-*/* | "$project"/build/* | */_deps/*) exit 0 ;;
    "$project"/*) ;;
    *) exit 0 ;;
esac

cd "$project" || exit 0

if git ls-files --error-unmatch -- "$file" >/dev/null 2>&1; then
    command -v git-clang-format >/dev/null 2>&1 || exit 0
    # --force: правки Claude не проиндексированы, без флага git clang-format откажется.
    git clang-format --force --quiet -- "$file" >/dev/null 2>&1
else
    command -v clang-format >/dev/null 2>&1 || exit 0
    clang-format -i "$file" >/dev/null 2>&1
fi

exit 0

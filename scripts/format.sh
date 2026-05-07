#!/bin/bash

PROJECT_ROOT=$(dirname "$(readlink -f "$0")")/..
cd "$PROJECT_ROOT"

echo "Formatting Iris source files..."

find app src include \
    \( -name "*.cpp" -o -name "*.hpp" \) \
    -not -path "*/build/*" \
    -print0 | xargs -0 clang-format -i

echo "Done! All files formatted."
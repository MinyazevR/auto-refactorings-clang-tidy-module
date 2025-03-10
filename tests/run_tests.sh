#!/bin/bash
set -xeuo pipefail

TESTS_DIR_ROOT=$(cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)
CLANG_TIDY_EXECUTABLE="$1"

for i in $(find "$TESTS_DIR_ROOT" -type d -name "*-tests")
do
  "$TESTS_DIR_ROOT/test.sh" "$i" "$CLANG_TIDY_EXECUTABLE" "$i"
done 

#!/bin/bash
set -xeuo pipefail

TESTS_DIR="$1"
CLANG_TIDY_EXECUTABLE="$2"
CLANG_TIDY_CONFIG_DIR="$3"
ADITIONAL_OPTIONS=${ADITIONAL_OPTIONS:-""}

TMP_TEST_DIR="$TESTS_DIR/tmp"

mkdir -p "$TMP_TEST_DIR"

cp "$TESTS_DIR/"*.c "$TMP_TEST_DIR"

for i in $(ls "$TMP_TEST_DIR/"*input*)
do
  echo "Process file $i"
  
  "$CLANG_TIDY_EXECUTABLE" --config-file="$CLANG_TIDY_CONFIG_DIR/.clang-tidy" --fix "$i"
  OUTPUT_FILE="${i/input/output}"
  
  if [ -f "$CLANG_TIDY_CONFIG_DIR/.post-clang-tidy" ]; then
   echo "Proccess .post-clang-tidy"
   "$CLANG_TIDY_EXECUTABLE" --config-file="$CLANG_TIDY_CONFIG_DIR/.post-clang-tidy" --fix "$i"
  fi
  
  if cmp -s "$i" "$OUTPUT_FILE"; then
    echo "Test for file "$i" with config $CLANG_TIDY_CONFIG_DIR/.clang-tidy passed"
  else
    echo "Test for file "$i" with config $CLANG_TIDY_CONFIG_DIR/.clang-tidy failed"
    exit 1
  fi
done


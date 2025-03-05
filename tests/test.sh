#!/bin/bash
set -xeuo pipefail

TESTS_DIR="$1"
CLANG_TIDY_EXECUTABLE="$2"
CLANG_TIDY_CONFIG="$3"

TMP_TEST_DIR="$TESTS_DIR/tmp"

mkdir -p "$TMP_TEST_DIR"

cp "$TESTS_DIR/"*.c "$TMP_TEST_DIR"

for i in $(ls "$TMP_TEST_DIR/"*input*)
do
  echo "Process file $i"
  
  "$CLANG_TIDY_EXECUTABLE" --config-file="$CLANG_TIDY_CONFIG" --fix "$i"
  OUTPUT_FILE="${i/input/output}"
  
  if cmp -s "$i" "$OUTPUT_FILE"; then
    echo "Test for file "$i" with config $CLANG_TIDY_CONFIG passed"
  else
    echo "Test for file "$i" with config $CLANG_TIDY_CONFIG failed"
    diff "$i" "$OUTPUT_FILE"
    exit 1
  fi
done


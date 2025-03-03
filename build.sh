#!/bin/bash
set -xeuo pipefail

LLVM_VERSION="${LLVM_VERSION:-master}"
ROOT_DIRECTORY=""

if [ "$LLVM_VERSION" = "master" ]; then
  git clone https://github.com/llvm/llvm-project.git
  ROOT_DIRECTORY=llvm-project
else
  wget -O llvm-project.tar.gz "https://github.com/llvm/llvm-project/archive/refs/tags/llvmorg-$LLVM_VERSION.tar.gz"
  tar -xf llvm-project.tar.gz
  ROOT_DIRECTORY="llvm-project-llvmorg-$LLVM_VERSION"
fi

pushd "$ROOT_DIRECTORY"
CLANG_TIDY_MODULES_RELATIVE_PATH="./clang-tools-extra/clang-tidy/"
CLANG_TIDY_MODULE_NAME="${CLANG_TIDY_MODULE_NAME:-autorefactorings}"
CLANG_TIDY_MODULE_PATH="$CLANG_TIDY_MODULES_RELATIVE_PATH/$CLANG_TIDY_MODULE_NAME"

mkdir -p "$CLANG_TIDY_MODULE_PATH"
cp -r ./src/* "$CLANG_TIDY_MODULE_PATH"

patch -p1 < llvm-project.patch

cmake -S llvm -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" \
    -DENABLE_ASAN=ON -DLLVM_CCACHE_BUILD="${ENABLE_ASAN:-OFF}" -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra"

cmake --build build/ -j$(nproc) --target clang-tidy --config "${BUILD_TYPE:-Release}"

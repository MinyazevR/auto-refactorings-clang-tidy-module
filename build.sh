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

CLANG_TIDY_MODULES_RELATIVE_PATH="$ROOT_DIRECTORY/clang-tools-extra/clang-tidy/"
CLANG_TIDY_MODULE_NAME="${CLANG_TIDY_MODULE_NAME:-autorefactorings}"
CLANG_TIDY_MODULE_PATH="$CLANG_TIDY_MODULES_RELATIVE_PATH/$CLANG_TIDY_MODULE_NAME"

mkdir -p "$CLANG_TIDY_MODULE_PATH"
cp -r ./src/* "$CLANG_TIDY_MODULE_PATH"

pushd "$ROOT_DIRECTORY"
patch -p1 < ../llvm-project.patch

cmake -S llvm -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" \
    -DENABLE_ASAN=${ENABLE_ASAN:-OFF} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DLLVM_CCACHE_BUILD=ON -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra"

cmake --build build/ -j$(nproc) --target clang-tidy --config "${BUILD_TYPE:-Release}"

popd

echo "CLANG_TIDY_BIN=$ROOT_DIRECTORY/build/bin/clang-tidy" >> $GITHUB_ENV
echo "COMPILE_COMMANDS_PATH=$ROOT_DIRECTORY/build" >> $GITHUB_ENV

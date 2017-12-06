#! /bin/bash
LLVM_ROOT=${1:-/src/llvm-build}
export PATH=$LLVM_ROOT/bin:$PATH
export LD_LIBRARY_PATH=$LLVM_ROOT/lib:$LD_LIBRARY_PATH
export INCLUDE=$LLVM_ROOT/include:$INCLUDE
cmake -G Ninja ..

@echo off

LLVM_ROOT=d:\llvm-build
PATH=%PATH%;%LLVM_ROOT%

cmake -DCMAKE_CXX_FLAGS="-DCLANG_38" -G "Visual Studio 12 2013" ..
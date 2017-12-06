@echo off

set LLVM_ROOT=d:\v3.1.build.work\llvm-build
set PATH=%PATH%;%LLVM_ROOT%

cmake -DCMAKE_CXX_FLAGS="-DCLANG_38" -G "Visual Studio 12 2013" ..
@echo off

SET dest=%1

pushd build-dir
cmake -DCMAKE_INSTALL_PREFIX=%dest% -P cmake_install.cmake
popd

xcopy /Y driver\bin\x64\Release\* %dest%\bin


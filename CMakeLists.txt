cmake_minimum_required(VERSION 3.1)
project(tidy-tools)

#set(CMAKE_DEBUG_TARGET_PROPERTIES INCLUDE_DIRECTORIES COMPILE_DEFINITIONS)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AddCXXCompilerFlag)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES ".*Clang")
   add_cxx_compiler_flag(-std=c++1z)
   add_cxx_compiler_flag(-Wall)
   # add_cxx_compiler_flag(-Wextra)
   # add_cxx_compiler_flag(-Wshadow)
   add_cxx_compiler_flag(-Werror RELEASE)
   add_cxx_compiler_flag(-Werror RELWITHDEBINFO)
   add_cxx_compiler_flag(-Werror MINSIZEREL)
   # add_cxx_compiler_flag(-pedantic)
   # add_cxx_compiler_flag(-pedantic-errors)
   add_cxx_compiler_flag(-fstrict-aliasing)
   add_cxx_compiler_flag(-Wthread-safety)
   add_cxx_compiler_flag(-fno-exceptions)
   add_cxx_compiler_flag(-fno-rtti)
   add_cxx_compiler_flag(-fvisibility-inlines-hidden)
   add_cxx_compiler_flag(-fvisibility=hidden)
   add_cxx_compiler_flag(-O3 RELEASE)
   add_cxx_compiler_flag(-stdlib=libc++)
elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
   add_cxx_compiler_flag(-MP)
endif()


find_package(LLVM REQUIRED CONFIG)
find_package(Clang)

macro(add_tidy_executable name)
   add_executable(${name} ${ARGN})
   target_include_directories(${name} PRIVATE ${LLVM_INCLUDE_DIRS})
   set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
   install(TARGETS ${name} RUNTIME DESTINATION bin)
endmacro()

macro(add_tidy_library name)
   add_library(${name} ${ARGN})
   target_include_directories(${name} PRIVATE ${LLVM_INCLUDE_DIRS})
endmacro()



add_subdirectory(tools)

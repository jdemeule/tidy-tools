
add_tidy_library(common-tidy STATIC
   Transform.cpp
   Transform.hpp
   misc.hpp)


target_link_libraries(common-tidy
   PUBLIC
   clangAST
   clangASTMatchers
   clangBasic
   clangFrontend
   clangTooling)

target_include_directories(common-tidy
   PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

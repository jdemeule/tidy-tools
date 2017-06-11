add_tidy_executable(clang-why
   main.cpp
   )

target_link_libraries(clang-why
   PRIVATE
   clangAST
   clangASTMatchers
   clangBasic
   clangFrontend
   clangTooling
   clangIndex
   )

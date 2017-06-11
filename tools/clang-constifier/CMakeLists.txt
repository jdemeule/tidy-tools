add_tidy_executable(clang-constifier
   ClangConstifier.cpp
   ConstifyVisitor.hpp
   ConstifyVisitor.cpp
   UseDefNode.hpp
   utils.hpp
   Graph.hpp
   )

target_link_libraries(clang-constifier
   PRIVATE
   clangAST
   clangASTMatchers
   clangBasic
   clangFrontend
   clangTooling
   )

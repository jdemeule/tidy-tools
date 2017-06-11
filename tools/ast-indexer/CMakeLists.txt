add_tidy_executable(ast-indexer
   AstIndexer.cpp
   )

target_link_libraries(ast-indexer
   PRIVATE
   clangAST
   clangASTMatchers
   clangBasic
   clangFrontend
   clangTooling
   clangIndex
   )

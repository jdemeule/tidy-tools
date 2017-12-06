add_tidy_executable(small-tidy
   EarlyReturn.cpp
   InitAtDeclare.cpp
   NonAsciiLiteral.cpp
   Sample.cpp
   SmallTidyMain.cpp)

target_link_libraries(small-tidy
   PRIVATE
   clangAST
   clangASTMatchers
   clangBasic
   clangFrontend
   clangTooling
   common-tidy)

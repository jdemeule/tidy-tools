
add_tidy_executable(small-tidy
   EarlyReturn.cpp
   InitAtDeclare.cpp
   NonAsciiLiteral.cpp
   Sample.cpp
   Transform.cpp
   Transform.hpp
   main.cpp
   misc.hpp
   )

target_link_libraries(small-tidy
   PRIVATE
   clangAST
   clangASTMatchers
   clangBasic
   clangFrontend
   clangTooling
   )

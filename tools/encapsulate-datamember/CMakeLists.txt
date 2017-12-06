add_tidy_executable(encapsulate-datamember
   EncapsulateDataMember.cpp
   EncapsulateDataMember.hpp
   EncapsulateDataMemberMain.cpp)

target_link_libraries(encapsulate-datamember
   PRIVATE
   clangAST
   clangASTMatchers
   clangBasic
   clangFrontend
   clangTooling
   common-tidy)

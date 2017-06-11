#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"

#include "Transform.hpp"

using namespace clang;
using namespace clang::tooling;

using namespace tidy;


int main(int argc, const char** argv) {
   Transforms transforms;
   transforms.registerOptions();

   CommonOptionsParser op(argc, argv, SmallTidyCategory);

   transforms.apply(op.getCompilations(), op.getSourcePathList());

   return 0;
}

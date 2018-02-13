//
// MIT License
//
// Copyright (c) 2017 Jeremy Demeule
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "EncapsulateDataMember.hpp"

#include <Transform.hpp>

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"

#include <iostream>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

using namespace tidy;

namespace {

static cl::OptionCategory Category("encapsulate datamember options");

static cl::list<std::string> Names(
   "names", cl::CommaSeparated,
   cl::desc("The list of fully qualified datamember to encapsulate, "
            "e.g. \"Foo::xxx,a::Bar::yyy\"."),
   cl::cat(Category));

static cl::opt<CaseLevel> Case(
   cl::desc("Choose getter/setter case:"),
   cl::values(clEnumValN(CaseLevel::camel, "camelCase",
                         "pascal case getX/setX (default)."),
              clEnumValN(CaseLevel::snake, "snake_case",
                         "bjarne case get_x/set_x.")
#if defined(CLANG_38)
                 ,
              clEnumValEnd
#endif
              ),
   cl::init(CaseLevel::camel),
   cl::cat(Category));

static cl::opt<bool> DontGenerateGetterSetter(
   "no-accessors-gen",
   cl::desc("Do not generated accessors (getter and setter) on given symbol. "
            "Expect they exist with the correct namming scheme (camel or snake "
            "case)."),
   cl::cat(Category));


static cl::opt<bool> WithNonConstGetter(
   "with-non-const-getter",
   cl::desc("Allow transformation using getter returning a reference."),
   cl::cat(Category));


static cl::opt<bool> Quiet("quiet", cl::desc("Discard clang warnings."),
                           cl::cat(Category));

static cl::opt<bool> StdOut("stdout",
                            cl::desc("Print output to cout instead of file"),
                            cl::cat(Category));
static cl::opt<bool> Export("export", cl::desc("Export fixes to patches"),
                            cl::cat(Category));

static cl::opt<std::string> OutputDir("outputdir",
                                      cl::desc("<path> output dir."),
                                      cl::cat(Category));

static std::string GetOutputDir() {
   if (OutputDir.empty())
      return "";

   std::string path = OutputDir;
   if (std::error_code EC = sys::fs::create_directory(path)) {
      std::cerr << "Error when create output directory (" << EC.value() << ")";
   }
   return path;
}

}  // namespace


int main(int argc, const char** argv) {
   CommonOptionsParser op(argc, argv, Category);
   RefactoringTool     Tool(op.getCompilations(), op.getSourcePathList());


   auto diagConsumer = llvm::make_unique<IgnoringDiagConsumer>();
   if (Quiet)
      Tool.setDiagnosticConsumer(diagConsumer.get());

   EncapsulateDataMemberOptions opts;
   opts.Names                = {Names.begin(), Names.end()};
   opts.Case                 = Case;
   opts.GenerateGetterSetter = !DontGenerateGetterSetter;
   opts.WithNonConstGetter   = WithNonConstGetter;

   TransformContext      ctx;
   EncapsulateDataMember action(&ctx, &opts);

   Transform::MatchFinder Finder;

   action.registerMatchers(&Finder);

   int res = Tool.run(newFrontendActionFactory(&Finder).get());

   if (StdOut)
      ctx.PrintReplacements(std::cout, Tool);
   if (Export)
      ctx.ExportReplacements(GetOutputDir());

   return res;
}

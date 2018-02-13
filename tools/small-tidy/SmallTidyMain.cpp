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
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "Transform.hpp"

#include "clang/Tooling/CommonOptionsParser.h"

#include <iostream>
#include <string>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace tidy;

namespace {

static cl::OptionCategory SmallTidyCategory("small tidy code options");


static cl::opt<bool> Quiet("quiet", cl::desc("Discard clang warnings."),
                           cl::cat(SmallTidyCategory));

static cl::opt<bool> StdOut("stdout",
                            cl::desc("Print output to cout instead of file"),
                            cl::cat(SmallTidyCategory));
static cl::opt<bool> Export("export", cl::desc("Export fixes to patches"),
                            cl::cat(SmallTidyCategory));

static cl::opt<bool> AllTransformation("all",
                                       cl::desc("Apply all transformations"),
                                       cl::cat(SmallTidyCategory));

static cl::opt<std::string> OutputDir("outputdir",
                                      cl::desc("<path> output dir."),
                                      cl::cat(SmallTidyCategory));

std::string GetOutputDir() {
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
   Transforms transforms;
   transforms.registerOptions(SmallTidyCategory);

   CommonOptionsParser op(argc, argv, SmallTidyCategory);

   transforms.apply(op.getCompilations(), op.getSourcePathList(), Quiet, StdOut,
                    Export, GetOutputDir());

   return 0;
}

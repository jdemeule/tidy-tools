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

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Index/USRGeneration.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <memory>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;


static cl::OptionCategory IndexerCategory("Indexer");

static cl::opt<bool> Quiet("quiet",
                           cl::desc("Do not report compiler warnings."));


template <typename NodeT>
bool isFromSystemHeader(const NodeT& Node, const SourceManager& SM) {
   auto ExpansionLoc = SM.getExpansionLoc(Node.getLocStart());
   if (ExpansionLoc.isInvalid()) {
      return false;
   }
   return SM.isInSystemHeader(ExpansionLoc);
}

std::string getUSRForDecl(const Decl* Decl) {
   SmallVector<char, 128> Buff;

   // FIXME: Add test for the nullptr case.
   if (Decl == nullptr || index::generateUSRForDecl(Decl, Buff))
      return "";

   return std::string(Buff.data(), Buff.size());
}

class IndexerVisitor : public RecursiveASTVisitor<IndexerVisitor> {
   // FunctionDecl
   // ClassDecl
   // EnumDecl
   // TypedefDecl
public:
   explicit IndexerVisitor(CompilerInstance& CI)
      : SM(CI.getSourceManager()) {}

   bool VisitVarDecl(VarDecl* D) {
      if (isFromSystemHeader(*D, SM))
         return true;

      if (D->getName().empty())
         return true;

      auto usr      = getUSRForDecl(D);
      auto location = D->getLocation();
      auto spelling = D->getNameAsString();
      auto type     = D->getType();
      auto typeStr  = type.getAsString();

      std::cout << "VarDecl: " << spelling << " -> " << typeStr << " @ "
                << location.printToString(SM) << " USR: " << usr << std::endl;
      return true;
   }

   bool VisitDeclRefExpr(DeclRefExpr* DR) {
      if (isFromSystemHeader(*DR, SM))
         return true;

      const auto* D    = DR->getDecl();
      const auto  usrD = getUSRForDecl(D);

      std::cout << "DeclRefExpr: @" << DR->getLocation().printToString(SM)
                << " to " << usrD << std::endl;

      return true;
   }

   SourceManager& SM;
};

class IndexerConsumer : public ASTConsumer {
public:
   explicit IndexerConsumer(CompilerInstance& CI)
      : Visitor(CI) {}

   bool HandleTopLevelDecl(DeclGroupRef DG) override {
      std::for_each(DG.begin(), DG.end(),
                    [&](Decl* decl) { Visitor.TraverseDecl(decl); });
      return true;
   }

   IndexerVisitor Visitor;
};

class IndexerFrontendAction : public ASTFrontendAction {
public:
   std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI,
                                                  StringRef file) override {
      return llvm::make_unique<IndexerConsumer>(CI);
   }
};

int main(int argc, const char** argv) {
   CommonOptionsParser op(argc, argv, IndexerCategory);
   ClangTool           Tool(op.getCompilations(), op.getSourcePathList());

   std::unique_ptr<IgnoringDiagConsumer> diagConsumer;
   if (!Quiet)
      diagConsumer.reset(new IgnoringDiagConsumer());

   Tool.setDiagnosticConsumer(diagConsumer.get());

   int res = Tool.run(newFrontendActionFactory<IndexerFrontendAction>().get());
   return res;
}

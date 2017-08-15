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

/*
 * Simple tool to print the overloads available during function selection.
 */

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Signals.h"

#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Sema/Lookup.h>
#include <clang/Sema/Sema.h>
#include <llvm/Support/raw_ostream.h>

#include <string>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

// Set up the command line options
static cl::extrahelp        CommonHelp(CommonOptionsParser::HelpMessage);
static cl::OptionCategory   ClangWhyCategory("clang-why options");
static cl::opt<std::string> FctName(
   "function",
   cl::desc("The name of the function to track overload selection"));

namespace {

typedef clang::CompilerInstance  CI;
typedef clang::DeclGroupRef      DGR;
typedef clang::DiagnosticsEngine DE;


struct Consumer : clang::ASTConsumer {
   Consumer(CI& c, std::string const& name)
      : c_(&c)
      , name_(name) {}
   bool HandleTopLevelDecl(clang::DeclGroupRef DG);
   CI*         c_;
   std::string name_;
};

struct Visitor : RecursiveASTVisitor<Visitor> {
   CI*         c_;
   std::string name_;
   Visitor(CI* c, std::string const& name)
      : c_(c)
      , name_(name) {}

   bool VisitCallExpr(CallExpr* d);
};

bool Visitor::VisitCallExpr(CallExpr* c) {
   FunctionDecl* fun = c->getDirectCallee();
   if (fun && fun->getNameAsString() == name_) {

      SourceLocation w(c->getExprLoc());
      DE&            de(this->c_->getDiagnostics());
      int            id = de.getCustomDiagID(DE::Warning, "function call: %0");
      int            info = de.getCustomDiagID(DE::Note, "function called");
      DiagnosticBuilder(de.Report(w, id)) << fun->getNameAsString();
      DiagnosticBuilder(de.Report(fun->getLocStart(), info))
         << fun->getNameAsString();

      int ref = de.getCustomDiagID(DE::Note, "previous decl");
      for (auto previous = fun->getPreviousDecl(); previous;
           previous      = previous->getPreviousDecl()) {
         DiagnosticBuilder(de.Report(previous->getLocStart(), ref));
      }

      Sema&        sema = this->c_->getSema();
      LookupResult result(sema, fun->getDeclName(), w, Sema::LookupAnyName);
      DeclContext* context = fun->getDeclContext();
      if (sema.LookupName(result, sema.getScopeForContext(context))) {
         int over = de.getCustomDiagID(DE::Note, "function overload");
         LookupResult::Filter filter = result.makeFilter();
         while (filter.hasNext()) {
            DiagnosticBuilder(de.Report(filter.next()->getLocStart(), over));
         }
         filter.done();
      }
   }
   // else {
   //    // I think the callee was a function object or a function pointer
   //}

   return true;
}

void doDecl(Consumer* c, Decl* d) {
   Visitor(c->c_, c->name_).TraverseDecl(d);
}

bool Consumer::HandleTopLevelDecl(DeclGroupRef DG) {
   std::for_each(DG.begin(), DG.end(),
                 std::bind1st(std::ptr_fun(&doDecl), this));
   return true;
}

class ConsumerAction : public ASTFrontendAction {
public:
   std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI,
                                                  StringRef file) override {
      return std::unique_ptr<ASTConsumer>(new Consumer(CI, FctName));
   }
};

}  // end anonymous namespace



int main(int argc, const char** argv) {
   //  llvm::sys::PrintStackTraceOnErrorSignal();
   CommonOptionsParser OptionsParser(argc, argv, ClangWhyCategory);

   if (FctName.empty())
      return 1;

   RefactoringTool Tool(OptionsParser.getCompilations(),
                        OptionsParser.getSourcePathList());
   return Tool.run(newFrontendActionFactory<ConsumerAction>().get());
}

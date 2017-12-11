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
// all
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

#include <iostream>
#include <sstream>

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Refactoring.h"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace tidy {

template <typename T>
static llvm::StringRef CodeFragment(const T& Node, const SourceManager& SM,
                                    const LangOptions& LangOpts) {
   return Lexer::getSourceText(
      CharSourceRange::getTokenRange(Node.getLocStart(), Node.getLocEnd()), SM,
      LangOpts);
}


class ReplaceMemcpy : public Transform {
public:
   ReplaceMemcpy(llvm::StringRef CheckName, TransformContext* ctx)
      : Transform(CheckName, ctx) {}

   virtual void registerMatchers(MatchFinder* Finder) {
      Finder->addMatcher(callExpr(callee(functionDecl(hasName("memcpy"))),
                                  hasArgument(2, unaryExprOrTypeTraitExpr()),
                                  isExpansionInMainFile())
                            .bind("memcpy"),
                         this);
   }

   virtual void check(const MatchFinder::MatchResult& Result) {
      auto callMemcpy = Result.Nodes.getNodeAs<CallExpr>("memcpy");
      auto Diag =
         diag(Result,
              callMemcpy->getExprLoc(),
              "Consider replacing `memcpy(x, y, sizeof(T))` with copy.");

      auto dst = callMemcpy->getArg(0);
      auto src = callMemcpy->getArg(1);


      std::string              buffer;
      llvm::raw_string_ostream fragment(buffer);

      printArgumentReplacement(fragment, dst, Result);
      fragment << " = ";
      printArgumentReplacement(fragment, src, Result);

      Diag << FixItHint::CreateReplacement(
         CharSourceRange::getCharRange(
            callMemcpy->getLocStart(),
            callMemcpy->getLocEnd().getLocWithOffset(1)),
         fragment.str());
   }

   void printArgumentReplacement(llvm::raw_ostream& out, const Expr* argument,
                                 const MatchFinder::MatchResult& Result) const {
      if (auto Cast = dyn_cast<ImplicitCastExpr>(argument)) {
         argument = Cast->getSubExpr();
      }

      bool needStar = true;
      if (auto addressOf = dyn_cast<UnaryOperator>(argument)) {
         if (addressOf->getOpcode() == UO_AddrOf) {
            needStar = false;
            argument = addressOf->getSubExpr();
         }
      }

      out << (needStar ? "*(" : "")
          << CodeFragment(*argument, *Result.SourceManager,
                          Result.Context->getLangOpts())
          << (needStar ? ")" : "");
   }
};

struct ReplaceMemcpyTransformFactory : public TransformFactory {
   virtual ~ReplaceMemcpyTransformFactory() {}
   virtual std::unique_ptr<Transform> create(llvm::StringRef   CheckName,
                                             TransformContext* context) const {
      return llvm::make_unique<ReplaceMemcpy>(CheckName, context);
   }
};

static TransformFactoryRegistry::Add<ReplaceMemcpyTransformFactory> X_Memcpy(
   "replace-memcpy", "");
}

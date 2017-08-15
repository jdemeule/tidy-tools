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

#include <iostream>
#include <sstream>

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/Refactoring.h"

#include "llvm/Support/Path.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace tidy {

class Sample : public Transform {
public:
   Sample(llvm::StringRef CheckName, TransformContext* ctx)
      : Transform(CheckName, ctx) {}

   virtual void registerMatchers(MatchFinder* Finder) {
      Finder->addMatcher(
         callExpr(
            callee(functionDecl(hasName("foo"), parameterCountIs(2))),
            hasArgument(
               0, ignoringImpCasts(unaryExprOrTypeTraitExpr().bind("1st"))))
            .bind("call"),
         this);
   }

   virtual void check(const MatchFinder::MatchResult& Result) {

      auto* call = Result.Nodes.getNodeAs<CallExpr>("call");

      auto Diag = diag(Result,
                       call->getExprLoc(),
                       "Consider use of non deprecated version of foo");

      auto* TSize     = Result.Nodes.getNodeAs<UnaryExprOrTypeTraitExpr>("1st");
      auto  TType     = TSize->getArgumentType();
      auto  TType_str = TType.getAsString(Result.Context->getPrintingPolicy());

      //     std::cout << "T is " << TType_str << "\n";
      std::stringstream template_args;
      template_args << "<" << TType_str << ">";

      Diag << FixItHint::CreateInsertion(
         call->getLocStart().getLocWithOffset(std::string("foo").size()),
         template_args.str());

      auto* lastArg   = call->getArg(1);
      auto  charRange = CharSourceRange::getCharRange(TSize->getLocStart(),
                                                     lastArg->getLocStart());
      Diag << FixItHint::CreateRemoval(charRange);
   }
};

struct SampleTransformFactory : public TransformFactory {
   virtual ~SampleTransformFactory() {}
   virtual std::unique_ptr<Transform> create(
      llvm::StringRef CheckName, TransformContext* context) const {
      return llvm::make_unique<Sample>(CheckName, context);
   }
};

static TransformFactoryRegistry::Add<SampleTransformFactory> Zzz(
   "sample", "Sample transform (template)");
}  // namespace tidy

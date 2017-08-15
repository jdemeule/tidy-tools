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
#include "clang/AST/RecursiveASTVisitor.h"
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

using namespace tidy;



class ReturnStatementFinderASTVisitor
   : public clang::RecursiveASTVisitor<ReturnStatementFinderASTVisitor> {
public:
   ReturnStatementFinderASTVisitor() {}

   /// Find the components of an expression and place them in a ComponentVector.
   void findReturns(const clang::FunctionDecl* Fct) {
      TraverseDecl(const_cast<clang::FunctionDecl*>(Fct));
   }

   /// Accessor for Components.
   const std::vector<const ReturnStmt*>& getReturns() {
      return Returns;
   }

   friend class clang::RecursiveASTVisitor<ReturnStatementFinderASTVisitor>;

private:
   std::vector<const ReturnStmt*> Returns;

   bool VisitReturnStmt(clang::ReturnStmt* Return) {
      Returns.push_back(Return);
      return true;
   }
};

static bool isTransformableLoc(const SourceLocation& Loc) {
   return !Loc.isInvalid() && !Loc.isMacroID();
}


class EarlyReturn : public Transform {
public:
   EarlyReturn(llvm::StringRef CheckName, TransformContext* ctx)
      : Transform(CheckName, ctx) {}

   virtual void registerMatchers(MatchFinder* Finder) {
      Finder->addMatcher(
         ifStmt(allOf(unless(hasElse(anything())),
                      hasParent(compoundStmt(
                                   hasParent(functionDecl().bind("early-fct")))
                                   .bind("enclosing-if")),
                      hasThen(allOf(compoundStmt().bind("compound-if"),
                                    unless(hasDescendant(returnStmt()))))))
            .bind("early-if"),
         this);
   }

   virtual void check(const MatchFinder::MatchResult& Result) {
      auto scopeIf = Result.Nodes.getNodeAs<CompoundStmt>("compound-if");
      if (scopeIf->size() < 4)
         return;

      auto earlyIf     = Result.Nodes.getNodeAs<IfStmt>("early-if");
      auto enclosingIf = Result.Nodes.getNodeAs<CompoundStmt>("enclosing-if");
      auto pos =
         std::find(enclosingIf->body_begin(), enclosingIf->body_end(), earlyIf);
      if (std::distance(pos, enclosingIf->body_end()) > 2)
         return;

      auto earlyFct = Result.Nodes.getNodeAs<FunctionDecl>("early-fct");
      auto condLoc  = earlyIf->getCond()->getExprLoc();

      if (!isTransformableLoc(earlyIf->getIfLoc()) ||
          !isTransformableLoc(scopeIf->getRBracLoc()) ||
          !isTransformableLoc(condLoc))
         return;

      std::stringstream condTextBuffer;
      condTextBuffer << "!("
                     << clang::Lexer::getSourceText(
                           CharSourceRange::getTokenRange(
                              earlyIf->getCond()->getSourceRange()),
                           *Result.SourceManager, Result.Context->getLangOpts())
                           .str()
                     << ")";

      auto condText   = condTextBuffer.str();
      auto returnText = computeBestMatchReturn(earlyFct, Result);

      auto Diag = diag(Result, earlyIf->getIfLoc(),
                       "Could be transform to early return if.");
      Diag << FixItHint::CreateRemoval(scopeIf->getRBracLoc())
           << FixItHint::CreateReplacement(earlyIf->getCond()->getSourceRange(),
                                           condText)
           << FixItHint::CreateReplacement(scopeIf->getLBracLoc(),
                                           returnText + "\n");
   }

   std::string computeBestMatchReturn(
      const FunctionDecl* fct, const MatchFinder::MatchResult& Result) const {
      ReturnStatementFinderASTVisitor returnFinder;
      returnFinder.findReturns(fct);
      auto& returns = returnFinder.getReturns();
      if (returns.empty())
         return "return;";

      auto        lastReturn = returns[returns.size() - 1];
      std::string text       = clang::Lexer::getSourceText(
         CharSourceRange::getTokenRange(lastReturn->getSourceRange()),
         *Result.SourceManager, Result.Context->getLangOpts());
      return text + ";";
   }
};

struct EarlyReturnFactory : public TransformFactory {
   virtual ~EarlyReturnFactory() {}
   virtual std::unique_ptr<Transform> create(llvm::StringRef   CheckName,
                                             TransformContext* context) const {
      return llvm::make_unique<EarlyReturn>(CheckName, context);
   }
};

static TransformFactoryRegistry::Add<EarlyReturnFactory> X_EarlyReturn(
   "early-return",
   "Tranform function' ifs to early return where "
   "possible. Only small if statement (less than 4 "
   "inner statments) and without any return could "
   "be transform.");

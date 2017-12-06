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

EncapsulateDataMember::EncapsulateDataMember(
   TransformContext* ctx, const EncapsulateDataMemberOptions* Options)
   : Transform("encapsulate-datamember", ctx)
   , Options(Options) {}


void EncapsulateDataMember::registerMatchers(MatchFinder* Finder) {
   for (const auto& Name : Options->Names) {
      Finder->addMatcher(fieldDecl(hasName(Name)).bind("decl"),
                         this);  // add getter & setter

      Finder->addMatcher(
         binaryOperator(hasOperatorName("="),
                        hasLHS(memberExpr(member(hasName(Name))).bind("lhs")))
            .bind("binop"),
         this);  // f.x = 42; => f.setX(42);

      Finder->addMatcher(
         unaryOperator(hasUnaryOperand(memberExpr(member(hasName(Name)),
                                                  hasObjectExpression(
                                                     expr().bind("varaccess")))
                                          .bind("unary")),
                       anyOf(hasOperatorName("++"), hasOperatorName("--")))
            .bind("unaryop"),
         this);  // ++f.x; => f.setX(f.getX() + 1);

      Finder->addMatcher(
         memberExpr(
            member(hasName(Name)),
            unless(
               anyOf(hasParent(binaryOperator(
                        hasOperatorName("="),
                        hasLHS(memberExpr(member(hasName(Name)))))),
                     hasParent(unaryOperator(
                        hasUnaryOperand(memberExpr(member(hasName(Name)))),
                        anyOf(hasOperatorName("++"), hasOperatorName("--")))))))
            .bind("getter"),
         this);  // f.x => f.getX();
   }
}

void EncapsulateDataMember::check(const MatchFinder::MatchResult& Result) {
   if (auto declaration = Result.Nodes.getNodeAs<FieldDecl>("decl")) {
      auto name = declaration->getNameAsString();
      auto get  = getterName(declaration);
      auto set  = setterName(declaration);

      auto type = declaration->getType().getAsString(
         Result.Context->getPrintingPolicy());

      std::stringstream fragment;
      fragment << type << " " << name << ";\n"
               << type << " " << get << "() const { return " << name << "; }\n"
               << "void " << set << "(" << type << " value) { " << name
               << " = value; }\n";

      auto Diag =
         diag(Result, declaration->getLocation(), "Encapsulating foo::x");
      Diag << FixItHint::CreateReplacement(
         CharSourceRange::getTokenRange(
            declaration->getLocStart(),
            declaration->getLocEnd().getLocWithOffset(1)),
         fragment.str());
   }
   else if (auto lhs = Result.Nodes.getNodeAs<MemberExpr>("lhs")) {
      auto binop = Result.Nodes.getNodeAs<BinaryOperator>("binop");
      auto set   = setterName(lhs->getMemberDecl());

      std::stringstream fragment;
      fragment << set << "("
               << clang::Lexer::getSourceText(
                     CharSourceRange::getTokenRange(
                        binop->getRHS()->getSourceRange()),
                     *Result.SourceManager, Result.Context->getLangOpts())
                     .str()
               << ")";

      auto Diag = diag(Result, lhs->getExprLoc(), "Encapsulating foo::x");
      Diag << FixItHint::CreateReplacement(
         CharSourceRange::getTokenRange(lhs->getExprLoc(), binop->getLocEnd()),
         fragment.str());
   }
   else if (auto unary = Result.Nodes.getNodeAs<MemberExpr>("unary")) {
      auto accessExpr = Result.Nodes.getNodeAs<Expr>("varaccess");

      auto get = getterName(unary->getMemberDecl());
      auto set = setterName(unary->getMemberDecl());

      auto varaccess =
         clang::Lexer::getSourceText(
            CharSourceRange::getTokenRange(accessExpr->getSourceRange()),
            *Result.SourceManager, Result.Context->getLangOpts())
            .str() +
         (unary->isArrow() ? "->" : ".");

      auto unaryop = Result.Nodes.getNodeAs<UnaryOperator>("unaryop");

      std::stringstream fragment;
      fragment << varaccess << set << "(" << varaccess << get << "()";
      if (unaryop->getOpcode() == UO_PostInc ||
          unaryop->getOpcode() == UO_PreInc)
         fragment << " + 1)";
      else
         fragment << " - 1)";


      auto Diag = diag(Result, unary->getExprLoc(), "Encapsulating foo::x");
      Diag << FixItHint::CreateReplacement(unaryop->getSourceRange(),
                                           fragment.str());
   }
   else if (auto getter = Result.Nodes.getNodeAs<MemberExpr>("getter")) {
      auto              get = getterName(getter->getMemberDecl());
      std::stringstream fragment;
      fragment << get << "()";
      auto Diag = diag(Result, getter->getExprLoc(), "Encapsulating foo::x");
      Diag << FixItHint::CreateReplacement(getter->getExprLoc(),
                                           fragment.str());
   }
}


static std::string CamelGetterName(const NamedDecl* decl) {
   std::string getter = "get" + decl->getNameAsString();
   std::transform(std::next(getter.begin(), 3), getter.end(),
                  std::next(getter.begin(), 3), ::toupper);
   return getter;
}

static std::string CamelSetterName(const NamedDecl* decl) {
   std::string setter = "set" + decl->getNameAsString();
   std::transform(std::next(setter.begin(), 3), setter.end(),
                  std::next(setter.begin(), 3), ::toupper);
   return setter;
}

static std::string SnakeGetterName(const NamedDecl* decl) {
   return "get_" + decl->getNameAsString();
}

static std::string SnakeSetterName(const NamedDecl* decl) {
   return "set_" + decl->getNameAsString();
}

std::string EncapsulateDataMember::getterName(const NamedDecl* decl) const {
   switch (Options->Case) {
   case CaseLevel::snake:
      return SnakeGetterName(decl);
   case CaseLevel::camel:
      return CamelGetterName(decl);
   }
}

std::string EncapsulateDataMember::setterName(const NamedDecl* decl) const {
   switch (Options->Case) {
   case CaseLevel::snake:
      return SnakeSetterName(decl);
   case CaseLevel::camel:
      return CamelSetterName(decl);
   }
}


}  // namespace tidy

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

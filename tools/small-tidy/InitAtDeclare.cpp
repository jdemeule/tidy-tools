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


StatementMatcher evaluator_ref() {
   return declRefExpr(to(varDecl(hasName("evaluator")))).bind("ref");
}


class InitAtDeclare : public Transform {
public:
   InitAtDeclare(llvm::StringRef CheckName, TransformContext* ctx)
      : Transform(CheckName, ctx) {}

   virtual void registerMatchers(MatchFinder* Finder) {
      Finder->addMatcher(evaluator_ref(), this);
      Finder->addMatcher(functionDecl(hasName("legacy_function")).bind("fct"),
                         this);
   }

   virtual void check(const MatchFinder::MatchResult& Result) {
      // auto ref = Result.Nodes.getNodeAs<DeclRefExpr>("ref");
      // auto decl = ref->getDecl();

      // auto q0 = decl->getDeclContext();
      // auto q1 = ref->getQualifier();
      //
      // ref->dumpColor();
   }
};

struct InitAtDeclareFactory : public TransformFactory {
   virtual ~InitAtDeclareFactory() {}
   virtual std::unique_ptr<Transform> create(llvm::StringRef   CheckName,
                                             TransformContext* context) const {
      return llvm::make_unique<InitAtDeclare>(CheckName, context);
   }
};

static TransformFactoryRegistry::Add<InitAtDeclareFactory> X_InitAtDeclare(
   "init-at-declare", "Detect unitialized variable");

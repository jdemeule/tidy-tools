#include "Transform.hpp"

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

#include <iostream>
#include <sstream>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace tidy {

AST_MATCHER(StringLiteral, containsNonAsciiOrNull) {
   return Node.containsNonAsciiOrNull();
}

class NonAsciiLiteral : public Transform {
public:
   NonAsciiLiteral(llvm::StringRef CheckName, TransformContext* ctx)
      : Transform(CheckName, ctx) {}

   virtual void registerMatchers(MatchFinder* Finder) {
      Finder->addMatcher(
         stringLiteral(containsNonAsciiOrNull()).bind("nonascii"), this);
   }

   virtual void check(const MatchFinder::MatchResult& Result) {
      if (auto literal = Result.Nodes.getNodeAs<StringLiteral>("nonascii")) {
         auto Diag = diag(Result,
                          literal->getLocStart(),
                          "Non ascii char in string literal",
                          DiagnosticIDs::Warning);
         auto locEnd =
            literal->getLocationOfByte(literal->getByteLength(),
                                       *Result.SourceManager,
                                       Result.Context->getLangOpts(),
                                       Result.Context->getTargetInfo());
         auto rng = CharSourceRange::getCharRange(literal->getLocStart(),
                                                  locEnd.getLocWithOffset(1));

         std::string              buffer;
         llvm::raw_string_ostream OS(buffer);
         literal->outputString(OS);
         Diag << FixItHint::CreateReplacement(rng, OS.str());
      }
   }
};

struct NonAsciiLiteralTransformFactory : public TransformFactory {
   virtual ~NonAsciiLiteralTransformFactory() {}
   virtual std::unique_ptr<Transform> create(llvm::StringRef   CheckName,
                                             TransformContext* context) const {
      return llvm::make_unique<NonAsciiLiteral>(CheckName, context);
   }
};


static TransformFactoryRegistry::Add<NonAsciiLiteralTransformFactory> Lit(
   "nonascii-literal", "NonAsciiLiteral transform");
}  // namespace tidy

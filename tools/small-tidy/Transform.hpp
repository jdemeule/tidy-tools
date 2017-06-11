#ifndef TRANSFORM_HPP
#define TRANSFORM_HPP

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Registry.h"
#include "llvm/Support/YAMLTraits.h"

namespace tidy {

class TransformContext {
public:
   void push_back(const clang::tooling::Replacement& replacement) {
#if CLANG_38
      m_replacements.insert(replacement);
#else
      auto error_code = m_replacements.add(replacement);
      if (error_code) {
         std::cerr << "Cannot apply " << replacement.toString() << '\n';
      }
#endif
   }

   void ExportReplacements(const std::string& outputDir) const;

   void PrintReplacements(std::ostream&                    ostr,
                          clang::tooling::RefactoringTool& Tool) const;

private:
   clang::tooling::Replacements m_replacements;
};

class FixItHIntHelper {
public:
   FixItHIntHelper(clang::SourceManager* sm, TransformContext* t,
                   clang::DiagnosticBuilder diag)
      : SM(sm)
      , Ctx(t)
      , Diag(diag)
      , Hints() {}

   void push_back(const clang::FixItHint& Hint);

private:
   clang::SourceManager*         SM;
   TransformContext*             Ctx;
   clang::DiagnosticBuilder      Diag;
   std::vector<clang::FixItHint> Hints;
};

inline FixItHIntHelper& operator<<(FixItHIntHelper&        h,
                                   const clang::FixItHint& hint) {
   h.push_back(hint);
   return h;
}

class Transform : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
   Transform(llvm::StringRef CheckName, TransformContext* ctx)
      : CheckName(CheckName)
      , m_ctx(ctx) {}

   virtual ~Transform() {}

   virtual void registerMatchers(clang::ast_matchers::MatchFinder* Finder) {}

   virtual void check(
      const clang::ast_matchers::MatchFinder::MatchResult& Result) {}

   FixItHIntHelper diag(
      const clang::ast_matchers::MatchFinder::MatchResult& Result,
      clang::SourceLocation Loc, llvm::StringRef Description,
      clang::DiagnosticIDs::Level Level = clang::DiagnosticIDs::Remark);

private:
   void run(
      const clang::ast_matchers::MatchFinder::MatchResult& Result) override;

protected:
   std::string       CheckName;
   TransformContext* m_ctx;
};

struct TransformFactory {
   virtual ~TransformFactory() {}
   virtual std::unique_ptr<Transform> create(llvm::StringRef   CheckName,
                                             TransformContext* context) const {
      return std::unique_ptr<Transform>();
   }
};

typedef llvm::Registry<TransformFactory> TransformFactoryRegistry;

class Transforms {
public:
   Transforms();

   void registerOptions();

   void apply(const clang::tooling::CompilationDatabase& Compilations,
              const std::vector<std::string>&            SourcePaths);

private:
   void instanciateTransforms();

private:
   typedef std::vector<std::unique_ptr<Transform>> TransformsInstances;
   typedef std::map<std::string, std::unique_ptr<llvm::cl::opt<bool>>>
      OptionsMap;

   TransformsInstances m_transforms;
   OptionsMap          m_options;
   TransformContext    m_context;
};


extern llvm::cl::OptionCategory   SmallTidyCategory;
extern std::string                GetOutputDir();
}  // namespace tidy

#endif

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

#ifndef TRANSFORM_HPP
#define TRANSFORM_HPP

#include <iosfwd>
#include <map>
#include <memory>
#include <string>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Registry.h"
//#include "llvm/Support/YAMLTraits.h"

namespace tidy {

class TransformContext {
public:
   void push_back(const clang::tooling::Replacement& replacement);

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
   using MatchFinder = clang::ast_matchers::MatchFinder;

   Transform(llvm::StringRef CheckName, TransformContext* ctx)
      : CheckName(CheckName)
      , m_ctx(ctx) {}

   virtual ~Transform() {}

   virtual void registerMatchers(MatchFinder* Finder) {}

   virtual void check(const MatchFinder::MatchResult& Result) {}

   FixItHIntHelper diag(
      const MatchFinder::MatchResult& Result, clang::SourceLocation Loc,
      llvm::StringRef             Description,
      clang::DiagnosticIDs::Level Level = clang::DiagnosticIDs::Remark);

private:
   void run(const MatchFinder::MatchResult& Result) override;

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

   void registerOptions(const llvm::cl::cat& Category);

   void apply(const clang::tooling::CompilationDatabase& Compilations,
              const std::vector<std::string>&            SourcePaths,
              bool                                       Quiet     = false,
              bool                                       StdOut    = false,
              bool                                       Export    = false,
              std::string                                OutputDir = "");

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


}  // namespace tidy

#endif

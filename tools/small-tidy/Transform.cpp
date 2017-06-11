#include "Transform.hpp"
#include "misc.hpp"

#include <iostream>
#include <sstream>

#include "clang/AST/AST.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/ReplacementsYaml.h"

#include "llvm/Support/Path.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

using namespace llvm;

#ifndef CLANG_38
LLVM_INSTANTIATE_REGISTRY(tidy::TransformFactoryRegistry);
#endif

namespace tidy {


cl::OptionCategory SmallTidyCategory("small tidy code options");


static cl::opt<bool> Quiet(
   "quiet", cl::desc("Do not report colored AST in case of error"),
   cl::cat(SmallTidyCategory));

static cl::opt<bool> StdOut("stdout",
                            cl::desc("Print output to cout instead of file"),
                            cl::cat(SmallTidyCategory));
static cl::opt<bool> Export("export", cl::desc("Export fixes to patches"),
                            cl::cat(SmallTidyCategory));

static cl::opt<bool> AllTransformation("all",
                                       cl::desc("Apply all transformations"),
                                       cl::cat(SmallTidyCategory));

static cl::opt<std::string> OutputDir("outputdir",
                                      cl::desc("<path> output dir."),
                                      cl::cat(SmallTidyCategory));

std::string GetOutputDir() {
   if (OutputDir.empty())
      return "";

   std::string path = OutputDir;
   if (std::error_code EC = sys::fs::create_directory(path)) {
      std::cerr << "Error when create output directory (" << EC.value() << ")";
   }
   return path;
}



template <typename It>
TranslationUnitReplacements BuildTURs(const std::string& mainfilepath,
                                      const std::string& context,
                                      It                 firstReplacement,
                                      It                 lastReplacement) {
   TranslationUnitReplacements TURs;
   TURs.MainSourceFile = mainfilepath;
   TURs.Replacements.assign(firstReplacement, lastReplacement);
   return TURs;
}

static void WriteReplacements(const Replacements& replacements,
                              const std::string&  outputDir) {
   if (replacements.empty())
      return;

   auto mainfilepath = replacements.begin()->getFilePath();

   auto filename =
      replace_all(sys::path::filename(mainfilepath).str(), ".", "_");

   std::stringstream outputPath;
   outputPath << outputDir << "/" << filename << ".yaml";
   std::error_code EC;
   raw_fd_ostream  ReplacementsFile(outputPath.str().c_str(), EC,
                                   llvm::sys::fs::F_None);
   if (EC) {
      std::cerr << "Error opening file: " << EC.message() << "\n";
      return;
   }

   std::stringstream context;
   context << "modernize of " << static_cast<std::string>(mainfilepath);

   yaml::Output YAML(ReplacementsFile);
   auto turs = BuildTURs(mainfilepath, context.str(), replacements.begin(),
                         replacements.end());
   YAML << turs;
}

void TransformContext::ExportReplacements(const std::string& outputDir) const {
   WriteReplacements(m_replacements, outputDir);
}

void TransformContext::PrintReplacements(std::ostream&    ostr,
                                         RefactoringTool& Tool) const {
   LangOptions                           DefaultLangOptions;
   IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
   TextDiagnosticPrinter DiagnosticPrinter(llvm::errs(), &*DiagOpts);
   DiagnosticsEngine     Diagnostics(
      IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()), &*DiagOpts,
      &DiagnosticPrinter, false);
   SourceManager Sources(Diagnostics, Tool.getFiles());
   Rewriter      Rewrite(Sources, DefaultLangOptions);

   if (!tooling::applyAllReplacements(m_replacements, Rewrite)) {
      llvm::errs() << "Skipped some replacements.\n";
   }
   else {
      std::for_each(Rewrite.buffer_begin(), Rewrite.buffer_end(),
                    [&ostr](const Rewriter::buffer_iterator::value_type& x) {
                       llvm::raw_os_ostream out(ostr);
                       x.second.write(out);
                    });
   }
}



Transforms::Transforms()
   : m_transforms()
   , m_options()
   , m_context() {}

void Transforms::registerOptions() {
   for (TransformFactoryRegistry::iterator
           I = TransformFactoryRegistry::begin(),
           E = TransformFactoryRegistry::end();
        I != E;
        ++I) {

      m_options[I->getName()] = llvm::make_unique<cl::opt<bool>>(
         I->getName(), cl::desc(I->getDesc()), cl::cat(SmallTidyCategory));
   }
}

void Transforms::apply(const CompilationDatabase&      Compilations,
                       const std::vector<std::string>& SourcePaths) {
   // made transform context local.
   // link the refactoring tool or at least the map of file/replacement to the
   // transform context
   instanciateTransforms();

   RefactoringTool Tool(Compilations, SourcePaths);

   auto diagConsumer = llvm::make_unique<IgnoringDiagConsumer>();

   if (Quiet)
      Tool.setDiagnosticConsumer(diagConsumer.get());

   MatchFinder Finder;

   for (auto& t : m_transforms)
      t->registerMatchers(&Finder);

   Tool.run(newFrontendActionFactory(&Finder).get());

   if (StdOut)
      m_context.PrintReplacements(std::cout, Tool);

   if (Export)
      m_context.ExportReplacements(GetOutputDir());
}


void Transforms::instanciateTransforms() {
   for (TransformFactoryRegistry::iterator
           I = TransformFactoryRegistry::begin(),
           E = TransformFactoryRegistry::end();
        I != E;
        ++I) {

      if (*m_options[I->getName()] || AllTransformation) {
         auto factory = I->instantiate();
         auto check   = factory->create(I->getName(), &m_context);
         if (check)
            m_transforms.emplace_back(std::move(check));
      }
   }
}

void Transform::run(
   const clang::ast_matchers::MatchFinder::MatchResult& Result) {
   // Context->setSourceManager(Result.SourceManager);
   check(Result);
}

FixItHIntHelper Transform::diag(
   const clang::ast_matchers::MatchFinder::MatchResult& Result,
   SourceLocation Loc, StringRef Description, DiagnosticIDs::Level Level) {
   assert(Loc.isValid());

   const SourceManager& Sources = *Result.SourceManager;
   if (Sources.isInSystemHeader(Loc))
      return FixItHIntHelper(nullptr, nullptr, DiagnosticBuilder::getEmpty());

   DiagnosticsEngine& DiagEngine = Result.Context->getDiagnostics();

   unsigned ID = DiagEngine.getDiagnosticIDs()->getCustomDiagID(
      Level, (Description + " [" + CheckName + "]").str());
   return FixItHIntHelper(Result.SourceManager, m_ctx,
                          DiagEngine.Report(Loc, ID));
}

void FixItHIntHelper::push_back(const FixItHint& Hint) {
   if (!SM)
      return;

   Diag << Hint;
   Hints.push_back(Hint);

   Ctx->push_back(Replacement(*SM, Hint.RemoveRange, Hint.CodeToInsert));
}

}  // namespace tidy

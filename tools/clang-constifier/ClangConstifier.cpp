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

#include <algorithm>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <stack>
#include <string>
#include <utility>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/ReplacementsYaml.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/raw_ostream.h"

#include "Graph.hpp"
#include "utils.hpp"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;
using namespace constifier;

static cl::opt<bool> AstDump("ast-dump", cl::desc("Print AST."));
static cl::opt<bool> GraphDump("graph-dump",
                               cl::desc("Dump 'char*' variables graph."));
static cl::opt<bool> Inplace(
   "inplace", cl::desc("Apply modification directly on input files."));
static cl::opt<bool> StdOut("stdout",
                            cl::desc("Print modifications on stdout."));
static cl::opt<bool> Verbose(
   "verbose", cl::desc("Be more verbose by applying replacements."));
static cl::opt<bool> PropagateToStaticFunction(
   "with-static-fct",
   cl::desc("Propagate constification on static function return."));
static cl::opt<bool> Quiet(
   "quiet", cl::desc("Do not report colored AST in case of error"));

static cl::opt<std::string> OutputDir("outputdir",
                                      cl::desc("<path> output dir."));
static cl::opt<std::string> Prefix(
   "prefix", cl::desc("<prefix> replacement file prefix."));

cl::list<std::string> ExcludePaths("exclude", cl::desc("<path> [... <path>]"),
                                   cl::ZeroOrMore);

std::string GetOutputDir() {
   std::stringstream pathbuilder;
   if (!OutputDir.empty())
      pathbuilder << OutputDir;
   else
      pathbuilder << "const-replacements";
   std::string path = pathbuilder.str();
   if (std::error_code EC = sys::fs::create_directory(path)) {
      std::cerr << "Error when create output directory (" << EC.value() << ")";
   }
   return path;
}

static int GlobalIndex = 0;

namespace {

struct Parameter {
   std::string name;
   bool        chartype;
   bool        constqualifier;
};

std::pair<bool, bool> isCharstarConst(QualType qtype) {
   if (const PointerType* PT = dyn_cast<PointerType>(qtype)) {
      auto pointee = PT->getPointeeType();

      if (auto alias = dyn_cast<TypedefType>(pointee)) {
         auto namedalias = alias->getDecl()->getName();
         if (namedalias == "MIBOOLEAN")
            return std::make_pair(false, false);
      }

      SplitQualType T_split = pointee.split();
      return std::make_pair(pointee->isCharType(), T_split.Quals.hasConst());
   }
   return std::make_pair(false, false);
}


std::string varName(const Decl* d) {
   if (!d)
      return "anonymous";

   if (const NamedDecl* ND = dyn_cast<NamedDecl>(d)) {
      if (ND->getDeclName())
         return ND->getNameAsString();
   }
   return "anonymous";
}


std::string LocationPath(const SourceLocation& loc, const SourceManager& SM) {
   auto fileid    = SM.getFileID(loc);
   auto fileentry = SM.getFileEntryForID(fileid);
   if (!fileentry)
      return "";
   return fileentry->getName();
}


Expr* ExtractFinalExpr(Expr* e) {
   if (!e)
      return nullptr;

   while (true) {
      if (ImplicitCastExpr* implicitCast = dyn_cast<ImplicitCastExpr>(e)) {
         e = implicitCast->getSubExpr();
         continue;
      }
      if (ArraySubscriptExpr* arraySubscript =
             dyn_cast<ArraySubscriptExpr>(e)) {
         e = arraySubscript->getBase();
         continue;
      }
      if (ParenExpr* parenExpr = dyn_cast<ParenExpr>(e)) {
         e = parenExpr->getSubExpr();
         continue;
      }
      if (UnaryOperator* unaryOp = dyn_cast<UnaryOperator>(e)) {
         e = unaryOp->getSubExpr();
         continue;
      }
      break;
   }
   return e;
}

VarDecl* ExtractValueDecl(Expr* e) {
   e = ExtractFinalExpr(e);

   if (!e)
      return nullptr;

   if (DeclRefExpr* declRef = dyn_cast<DeclRefExpr>(e)) {
      ValueDecl* valueDecl = declRef->getDecl();
      if (VarDecl* vardecl = dyn_cast<VarDecl>(valueDecl)) {
         auto type = vardecl->getType();

         if (isCharstarConst(type).first)
            return vardecl;
         return nullptr;
      }
   }

   return nullptr;
}

std::vector<VarDecl*> ExtractValueDecls(Expr* e) {
   std::vector<VarDecl*> vars;
   e = ExtractFinalExpr(e);
   if (!e)
      return vars;

   if (auto ternary = dyn_cast<ConditionalOperator>(e)) {
      auto lhss = ExtractValueDecls(ternary->getLHS());
      std::copy(lhss.begin(), lhss.end(), std::back_inserter(vars));
      auto rhss = ExtractValueDecls(ternary->getRHS());
      std::copy(rhss.begin(), rhss.end(), std::back_inserter(vars));
   }

   if (DeclRefExpr* declRef = dyn_cast<DeclRefExpr>(e)) {
      ValueDecl* valueDecl = declRef->getDecl();
      if (VarDecl* vardecl = dyn_cast<VarDecl>(valueDecl)) {
         auto type = vardecl->getType();

         if (isCharstarConst(type).first)
            vars.push_back(vardecl);
      }
   }

   return vars;
}

template <typename T>
std::vector<T*> ExtractDecls(Expr* e) {
   std::vector<T*> vars;
   e = ExtractFinalExpr(e);
   if (!e)
      return vars;

   if (auto ternary = dyn_cast<ConditionalOperator>(e)) {
      auto lhss = ExtractDecls<T>(ternary->getLHS());
      std::copy(lhss.begin(), lhss.end(), std::back_inserter(vars));
      auto rhss = ExtractDecls<T>(ternary->getRHS());
      std::copy(rhss.begin(), rhss.end(), std::back_inserter(vars));
   }

   if (DeclRefExpr* declRef = dyn_cast<DeclRefExpr>(e)) {
      ValueDecl* valueDecl = declRef->getDecl();
      if (T* d = dyn_cast<T>(valueDecl)) {
         vars.push_back(d);
      }
   }

   return vars;
}

VarDecl* ExtractVoidPointerDecl(Expr* e) {
   e = ExtractFinalExpr(e);

   if (!e)
      return nullptr;

   if (DeclRefExpr* declRef = dyn_cast<DeclRefExpr>(e)) {
      ValueDecl* valueDecl = declRef->getDecl();
      if (VarDecl* vardecl = dyn_cast<VarDecl>(valueDecl)) {
         auto type = vardecl->getType();

         if (type->isVoidPointerType())
            return vardecl;
         return nullptr;
      }
   }

   return nullptr;
}

clang::StringLiteral* ExtractLiteral(Expr* e) {
   if (!e)
      return nullptr;

   while (ImplicitCastExpr* implicitCast = dyn_cast<ImplicitCastExpr>(e)) {
      e = implicitCast->getSubExpr();
   }

   if (!e)
      return nullptr;

   if (clang::StringLiteral* ST = dyn_cast<clang::StringLiteral>(e)) {
      return ST;
   }

   return nullptr;
}

CharacterLiteral* ExtractCharacterLiteral(Expr* e) {
   if (!e)
      return nullptr;

   while (ImplicitCastExpr* implicitCast = dyn_cast<ImplicitCastExpr>(e)) {
      e = implicitCast->getSubExpr();
   }

   if (!e)
      return nullptr;

   if (CharacterLiteral* CT = dyn_cast<CharacterLiteral>(e)) {
      return CT;
   }
   return nullptr;
}

IntegerLiteral* ExtractIntegerLiteral(Expr* e) {
   if (!e)
      return nullptr;

   while (ImplicitCastExpr* implicitCast = dyn_cast<ImplicitCastExpr>(e)) {
      e = implicitCast->getSubExpr();
   }

   if (!e)
      return nullptr;

   if (IntegerLiteral* IT = dyn_cast<IntegerLiteral>(e)) {
      return IT;
   }
   return nullptr;
}

FieldDecl* ExtractFieldDecl(Expr* e) {
   if (!e)
      return nullptr;

   if (MemberExpr* m = dyn_cast<MemberExpr>(e)) {
      auto memberDecl = m->getMemberDecl();
      if (!memberDecl)
         return nullptr;
      return dyn_cast<FieldDecl>(memberDecl);
   }
   return nullptr;
}

CallExpr* ExtractCallExpr(Expr* e) {
   if (!e)
      return nullptr;

   while (ImplicitCastExpr* implicitCast = dyn_cast<ImplicitCastExpr>(e)) {
      e = implicitCast->getSubExpr();
   }
   if (!e)
      return nullptr;
   if (CallExpr* call = dyn_cast<CallExpr>(e)) {
      return call;
   }
   return nullptr;
}

FunctionDecl* ExtractFunctionDecl(Expr* e) {
   if (!e)
      return nullptr;

   while (ImplicitCastExpr* implicitCast = dyn_cast<ImplicitCastExpr>(e)) {
      e = implicitCast->getSubExpr();
   }

   if (!e)
      return nullptr;

   if (auto declref = dyn_cast<DeclRefExpr>(e)) {
      auto decl = declref->getDecl();
      if (!decl)
         return nullptr;
      return dyn_cast<FunctionDecl>(decl);
   }

   return nullptr;
}


const FunctionProtoType* ExtractFunctionProto(const Decl* d) {
   auto declarator = dyn_cast<DeclaratorDecl>(d);
   if (!declarator)
      return nullptr;

   auto declarator_type = declarator->getType();
   auto typedef_type    = dyn_cast<TypedefType>(declarator_type);
   if (typedef_type) {
      auto typdefdecl = typedef_type->getDecl();
      declarator_type = typdefdecl->getUnderlyingType();
   }

   auto pointer_type = dyn_cast<PointerType>(declarator_type);
   if (!pointer_type)
      return nullptr;

   auto pointee_type = pointer_type->getPointeeType();
   auto parent_type  = dyn_cast<ParenType>(pointee_type);
   if (!parent_type)
      return nullptr;

   auto inner_type   = parent_type->getInnerType();
   auto f_proto_type = dyn_cast<FunctionProtoType>(inner_type);
   if (!f_proto_type)
      return nullptr;

   return f_proto_type;
}


class Function {
public:
   explicit Function(const FunctionDecl* fctdecl)
      : m_fct(fctdecl)
      , m_name()
      , m_parameters() {

      DeclarationName DeclName = m_fct->getNameInfo().getName();
      m_name                   = DeclName.getAsString();

      std::transform(
         m_fct->param_begin(), m_fct->param_end(),
         std::back_inserter(m_parameters), [](ParmVarDecl* p) {
            std::string named;

            if (p) {
               if (const NamedDecl* ND = dyn_cast<NamedDecl>(p)) {
                  if (ND->getDeclName())
                     named = ND->getNameAsString();
               }
            }

            auto charConst = isCharstarConst(p->getType());

            return Parameter{named, charConst.first, charConst.second};
         });
   }

   Function()
      : m_fct()
      , m_name()
      , m_parameters() {}

   Function(const Function& rhs)
      : m_fct(rhs.m_fct)
      , m_name(rhs.m_name)
      , m_parameters(rhs.m_parameters) {}

   Function& operator=(const Function& rhs) {
      Function tmp(rhs);
      swap(tmp);
      return *this;
   }

   void swap(Function& rhs) {
      std::swap(m_fct, rhs.m_fct);
      std::swap(m_name, rhs.m_name);
      std::swap(m_parameters, rhs.m_parameters);
   }

   std::string name() const {
      return m_name;
   }

   bool has_definition() const {
      return m_fct->hasBody();
   }

   bool is_declaration_pure() const {
      bool unpure = std::any_of(
         m_parameters.begin(), m_parameters.end(),
         [](const Parameter& p) { return p.chartype && !p.constqualifier; });
      return !unpure;
   }

   bool is_definition_pure() const {
      return false;
   }

   bool is_nth_parameter_charstar_pure(std::size_t n) const {
      if (n < m_parameters.size())
         return false;
      auto& p = m_parameters[n];
      return p.chartype && p.constqualifier;
   }

   bool is_static() const {
      return false;
   }

   const std::vector<Parameter> parameters() const {
      return m_parameters;
   }

   const Parameter& parameter_at(std::size_t index) const {
      return m_parameters[index];
   }

   std::size_t parameters_size() const {
      return m_parameters.size();
   }

private:
   const FunctionDecl*    m_fct;
   std::string            m_name;
   std::vector<Parameter> m_parameters;
};

enum class NodeType {
   Empty,
   LiteralAssignment,
   Modifier,
   Var,
   Field,
   Function,
   FunctionProtoTypeParameter,
   FunctionPointerParameter
};

inline std::ostream& operator<<(std::ostream& ostr, NodeType t) {
   switch (t) {
   case NodeType::Empty:
      ostr << "Empty";
      break;
   case NodeType::LiteralAssignment:
      ostr << "LiteralAssignment";
      break;
   case NodeType::Modifier:
      ostr << "Modifier";
      break;
   case NodeType::Var:
      ostr << "Var";
      break;
   case NodeType::Field:
      ostr << "Field";
      break;
   case NodeType::Function:
      ostr << "Function";
      break;
   case NodeType::FunctionProtoTypeParameter:
      ostr << "Prototype Parameter";
      break;
   case NodeType::FunctionPointerParameter:
      ostr << "Fonction pointer parameter";
      break;
   }
   return ostr;
}

class UseDefNode {
protected:
   UseDefNode(const Decl* decl, bool isConst, bool isModifiable = true)
      : m_decl(decl)
      , m_isConst(isConst)
      , m_modifiable(isModifiable) {}

public:
   virtual ~UseDefNode() {}

   virtual bool isConst() const {
      return m_isConst;
   }

   virtual bool isModifiable() const {
      return m_modifiable;
   }

   virtual bool isFwdDeclaration() const {
      return false;
   }

   virtual bool isConstifiable() const {
      return false;
   }

   virtual void constify(SourceManager&            SM,
                         std::vector<Replacement>& replacements) const {}

   virtual std::string getName() const {
      return "Empty";
   }

   virtual NodeType getType() const {
      return NodeType::Empty;
   }

   const Decl* getDecl() const {
      return m_decl;
   }

   virtual bool isComposite() const {
      return false;
   }

   virtual const FunctionDecl* getParentFunctionDecl() const {
      return nullptr;
   }

protected:
   void markAsConst() const {
      m_isConst = true;
   }

protected:
   const Decl*  m_decl;
   mutable bool m_isConst;
   bool         m_modifiable;
};

class VarNode : public UseDefNode {
public:
   explicit VarNode(const VarDecl* var, bool modifiable = true)
      : UseDefNode(var, isCharstarConst(var->getType()).second, modifiable)
      , m_name(varName(var)) {}

   virtual ~VarNode() {}

   virtual std::string getName() const {
      return m_name;
   }

   virtual NodeType getType() const {
      return NodeType::Var;
   }

   virtual bool isFwdDeclaration() const {
      auto parent = getDecl()->getParentFunctionOrMethod();
      if (!parent)
         return true;

      if (auto fct = dyn_cast<FunctionDecl>(parent)) {
         return !fct->isThisDeclarationADefinition() ||
                fct->isTemplateInstantiation() || fct->isInlined();
      }
      return false;
   }

   virtual bool isConstifiable() const {
      return !isFwdDeclaration() && isModifiable();
   }

   virtual bool isModifiable() const {
      return m_modifiable;
   }

   virtual void constify(SourceManager&            SM,
                         std::vector<Replacement>& replacements) const {
      if (m_isConst)
         return;

      markAsConst();

      auto decl = getDecl();
      replacements.push_back(
         Replacement(SM, decl->getSourceRange().getBegin(), 0, "const "));

      auto parent = decl->getParentFunctionOrMethod();
      if (!parent)
         return;

      if (auto fct = dyn_cast<FunctionDecl>(parent)) {
         auto declInPrototype =
            std::find(fct->param_begin(), fct->param_end(), decl);
         if (declInPrototype == fct->param_end())
            return;

         if (auto previous = fct->getPreviousDecl()) {
            auto index   = std::distance(fct->param_begin(), declInPrototype);
            auto fwddecl = previous->getParamDecl(index);
            replacements.push_back(Replacement(
               SM, fwddecl->getSourceRange().getBegin(), 0, "const "));
         }
      }
   }

   virtual const FunctionDecl* getParentFunctionDecl() const {
      if (!isFunctionParameter())
         return nullptr;
      auto parent = m_decl->getParentFunctionOrMethod();
      if (!parent)
         return nullptr;
      return dyn_cast<FunctionDecl>(parent);
   }

   bool isFunctionParameter() const {
      auto parent = m_decl->getParentFunctionOrMethod();
      if (!parent)
         return false;

      auto fundecl = dyn_cast<FunctionDecl>(parent);
      auto declInPrototype =
         std::find(fundecl->param_begin(), fundecl->param_end(), m_decl);
      return declInPrototype != fundecl->param_end();
   }

private:
   std::string m_name;
};

class ReadOnlyVarNode : public UseDefNode {
public:
   explicit ReadOnlyVarNode(const VarDecl* var)
      : UseDefNode(var, isCharstarConst(var->getType()).second, false)
      , m_name(varName(var)) {}

   virtual ~ReadOnlyVarNode() {}

   virtual std::string getName() const {
      return m_name;
   }

   virtual NodeType getType() const {
      return NodeType::Var;
   }

   virtual bool isFwdDeclaration() const {
      auto parent = getDecl()->getParentFunctionOrMethod();
      if (!parent)
         return true;

      if (auto fct = dyn_cast<FunctionDecl>(parent)) {
         return !fct->isThisDeclarationADefinition();
      }
      return false;
   }

   virtual bool isConstifiable() const {
      return false;
   }

   virtual void constify(SourceManager&            SM,
                         std::vector<Replacement>& replacements) const {}

private:
   std::string m_name;
};

class FieldNode : public UseDefNode {
public:
   explicit FieldNode(const FieldDecl* field)
      : UseDefNode(field, isCharstarConst(field->getType()).second, false)
      , m_name(varName(field)) {}

   virtual ~FieldNode() {}

   virtual std::string getName() const {
      return m_name;
   }

   virtual NodeType getType() const {
      return NodeType::Field;
   }

   virtual bool isFwdDeclaration() const {
      return false;
   }

   virtual bool isConstifiable() const {
      return false;
   }

   virtual void constify(SourceManager&            SM,
                         std::vector<Replacement>& replacements) const {}

private:
   std::string m_name;
};

class StringLiteralNode : public UseDefNode {
public:
   explicit StringLiteralNode(const clang::StringLiteral* l)
      : UseDefNode(nullptr, true, false)
      , m_l(l) {}

   virtual ~StringLiteralNode() {}

   virtual std::string getName() const {
      std::stringstream name;
      name << "__literal__ \"" << m_l->getString().str() << "\"";
      return name.str();
   }

   virtual NodeType getType() const {
      return NodeType::LiteralAssignment;
   }

   virtual bool isFwdDeclaration() const {
      return false;
   }

private:
   const clang::StringLiteral* m_l;
};

class IntegralLiteralNode : public UseDefNode {
public:
   explicit IntegralLiteralNode(const IntegerLiteral* l)
      : UseDefNode(nullptr, false, false)
      , m_l(l) {}

   virtual ~IntegralLiteralNode() {}

   virtual std::string getName() const {
      return "__int_modifier__";
   }

   virtual NodeType getType() const {
      return NodeType::Modifier;
   }

   virtual bool isFwdDeclaration() const {
      return false;
   }

private:
   const IntegerLiteral* m_l;
};

class IntegralLiteralConstNode : public UseDefNode {
public:
   explicit IntegralLiteralConstNode(const IntegerLiteral* l)
      : UseDefNode(nullptr, true, false)
      , m_l(l) {}

   virtual ~IntegralLiteralConstNode() {}

   virtual std::string getName() const {
      return "__int_const_modifier__";
   }

   virtual NodeType getType() const {
      return NodeType::Modifier;
   }

   virtual bool isFwdDeclaration() const {
      return false;
   }

private:
   const IntegerLiteral* m_l;
};

class CharacterLiteralNode : public UseDefNode {
public:
   explicit CharacterLiteralNode(const CharacterLiteral* l)
      : UseDefNode(nullptr, false, false)
      , m_l(l) {}

   virtual ~CharacterLiteralNode() {}

   virtual std::string getName() const {
      return "__char_modifier__";
   }

   virtual NodeType getType() const {
      return NodeType::Modifier;
   }

   virtual bool isFwdDeclaration() const {
      return false;
   }

private:
   const CharacterLiteral* m_l;
};

class CompositeNode : public UseDefNode {
public:
   explicit CompositeNode(std::vector<UseDefNode*>&& nodes)
      : UseDefNode(nodes.front()->getDecl(), false)
      , m_nodes(std::move(nodes)) {}

   virtual ~CompositeNode() {}

   virtual std::string getName() const {
      std::stringstream name;
      name << "(";
      std::transform(m_nodes.begin(), m_nodes.end(),
                     std::ostream_iterator<std::string>(name, " "),
                     [](UseDefNode* n) { return n->getName(); });
      name << ")";
      return name.str();
   }

   virtual NodeType getType() const {
      return std::accumulate(m_nodes.begin(), m_nodes.end(), NodeType::Empty,
                             [](NodeType type, const UseDefNode* n) {
                                if (type != NodeType::Field)
                                   type = n->getType();
                                return type;
                             });
   }

   virtual bool isConst() const {
      return std::accumulate(m_nodes.begin(), m_nodes.end(), true,
                             [](bool value, const UseDefNode* n) {
                                if (value && !n->isConst())
                                   value = false;
                                return value;
                             });
   }

   virtual bool isModifiable() const {
      return std::accumulate(m_nodes.begin(), m_nodes.end(), true,
                             [](bool value, const UseDefNode* n) {
                                if (value && !n->isModifiable())
                                   value = false;
                                return value;
                             });
   }

   virtual bool isFwdDeclaration() const {
      return std::accumulate(m_nodes.begin(), m_nodes.end(), false,
                             [](bool value, const UseDefNode* n) {
                                if (!value && n->isFwdDeclaration())
                                   value = true;
                                return value;
                             });
   }

   virtual bool isConstifiable() const {
      return !isFwdDeclaration() && isModifiable();
   }

   virtual void constify(SourceManager&            SM,
                         std::vector<Replacement>& replacements) const {
      markAsConst();
      for (auto x : m_nodes)
         x->constify(SM, replacements);
   }

private:
   std::vector<UseDefNode*> m_nodes;
};

class FunctionNode : public UseDefNode {
public:
   explicit FunctionNode(const FunctionDecl* fundecl)
      : UseDefNode(fundecl, isCharstarConst(fundecl->getReturnType()).second,
                   false)
      , m_function(fundecl) {}

   virtual ~FunctionNode() {}

   virtual std::string getName() const {
      std::stringstream name;
      name << m_function.name() << " RV";
      return name.str();
   }

   virtual NodeType getType() const {
      return NodeType::Function;
   }

   virtual bool isFwdDeclaration() const {
      return !m_function.has_definition();
   }

   virtual bool isConstifiable() const {
      // return !isFwdDeclaration() && isModifiable();
      // return !isstatic && PropagateToStaticFunction
      return false;
   }

   virtual void constify(SourceManager&            SM,
                         std::vector<Replacement>& replacements) const {
      // TODO
      // markAsConst();
   }

private:
   Function m_function;
};



QualType ExtractTypeof(const FunctionProtoType* prototype, int arg) {
   if (arg < 0)
      return prototype->getReturnType();
   return prototype->getParamType(arg);
}

class FunctionProtoTypeNode : public UseDefNode {
public:
   explicit FunctionProtoTypeNode(const FunctionProtoType* funproto, int narg)
      : UseDefNode(nullptr,
                   isCharstarConst(ExtractTypeof(funproto, narg)).second, false)
      , m_funproto(funproto)
      , m_narg(narg) {}

   virtual ~FunctionProtoTypeNode() {}

   virtual std::string getName() const {
      // auto t = m_funproto->getParamType(m_narg);
      //
      // std::stringstream name;
      // llvm::raw_os_ostream buffer(name);
      // LangOptions LO;
      // t.print(buffer, PrintingPolicy(LO), "identifier");

      // return name.str();
      if (m_narg < 0)
         return "__prototype_return__";
      return "__prototype_parameter__";
   }

   virtual NodeType getType() const {
      return NodeType::FunctionProtoTypeParameter;
   }

   virtual bool isFwdDeclaration() const {
      return true;
   }

   virtual bool isConstifiable() const {
      return false;
   }

   virtual void constify(SourceManager&            SM,
                         std::vector<Replacement>& replacements) const {
      // TODO
      // markAsConst();
   }

private:
   const FunctionProtoType* m_funproto;
   int                      m_narg;
};

class FunctionPointerParameterNode : public UseDefNode {
public:
   explicit FunctionPointerParameterNode(const FunctionDecl* method,
                                         std::size_t         narg)
      : UseDefNode(
           method,
           isCharstarConst(method->getParamDecl(narg)->getType()).second, false)
      , m_method(method)
      , m_narg(narg) {}

   virtual ~FunctionPointerParameterNode() {}

   virtual std::string getName() const {
      std::stringstream name;
      name << "(FctPtr)::" << m_method->getName().str();
      return name.str();
   }

   virtual NodeType getType() const {
      return NodeType::FunctionPointerParameter;
   }

   virtual bool isFwdDeclaration() const {
      return true;
   }

   virtual bool isConstifiable() const {
      return true;
   }

   virtual void constify(SourceManager&            SM,
                         std::vector<Replacement>& replacements) const {
      // TODO:
      // markAsConst();
      // add replacement
   }

private:
   // const FieldDecl* m_method;
   const FunctionDecl* m_method;
   std::size_t         m_narg;
};

class DummyModifierNode : public UseDefNode {
public:
   explicit DummyModifierNode()
      : UseDefNode(nullptr, false, false) {}

   virtual ~DummyModifierNode() {}

   virtual std::string getName() const {
      return "__dummy_modifier__";
   }

   virtual NodeType getType() const {
      return NodeType::Modifier;
   }

   virtual bool isFwdDeclaration() const {
      return false;
   }
};


inline std::ostream& operator<<(std::ostream& ostr, const UseDefNode& v) {
   ostr << v.getName() << (v.isConst() ? " (const)" : "");
   if (v.getType() == NodeType::Var || v.getType() == NodeType::Field)
      ostr << " /" << v.getType() << "/";
   if (v.isComposite())
      ostr << " (composite)";
   return ostr;
}

inline std::ostream& operator<<(std::ostream& ostr, const UseDefNode* v) {
   if (!v)
      return ostr << "(null)";
   return ostr << *v;
}

typedef DiGraph<UseDefNode*> UseDefGraph;

class NodeManager {
public:
   UseDefNode* find(const void* d) {
      auto found = m_declnodes.find(d);
      if (found == m_declnodes.end())
         return nullptr;
      return found->second.get();
   }

   UseDefNode* register_node(const void* d, std::unique_ptr<UseDefNode> node) {
      auto x = m_declnodes.insert(std::make_pair(d, std::move(node)));
      return x.first->second.get();
   }

   template <typename U, typename T, typename... Args>
   UseDefNode* nodeof(const T* p, Args&&... args) {
      UseDefNode* found = find(p);
      if (found)
         return found;
      return register_node(p,
                           std::make_unique<U>(p, std::forward<Args>(args)...));
   }

   UseDefNode* compositeNode(std::vector<UseDefNode*>&& nodes) {
      m_othernodes.push_back(std::make_unique<CompositeNode>(std::move(nodes)));
      return m_othernodes.back().get();
   }

   UseDefNode* functionPointerParameterNode(const FunctionDecl* method,
                                            std::size_t         arg) {
      m_othernodes.push_back(
         std::make_unique<FunctionPointerParameterNode>(method, arg));
      return m_othernodes.back().get();
   }

   UseDefNode* prototypeNode(const FunctionProtoType* prototype, int arg) {
      m_othernodes.push_back(
         std::make_unique<FunctionProtoTypeNode>(prototype, arg));
      return m_othernodes.back().get();
   }

   template <typename U, typename T>
   UseDefNode* literalNode(T* p) {
      m_othernodes.push_back(std::make_unique<U>(std::move(p)));
      return m_othernodes.back().get();
   }

   DummyModifierNode* dummyModifier() {
      return &m_dummyModifier;
   }

private:
   std::map<const void*, std::unique_ptr<UseDefNode>> m_declnodes;
   std::vector<std::unique_ptr<UseDefNode>>           m_othernodes;
   DummyModifierNode                                  m_dummyModifier;
};


void copy_substr(const std::string::const_iterator& start,
                 const std::string::const_iterator& end, std::ostream& ostr) {
   for (std::string::const_iterator it = start; it != end; ++it)
      ostr << *it;
}

std::string replace_all(const std::string& str, const std::string& what,
                        const std::string& with) {
   std::stringstream      sstr;
   std::string::size_type pos        = 0;
   std::string::size_type previous   = 0;
   std::string::size_type whatLength = what.length();

   while ((pos = str.find(what, pos)) != std::string::npos) {
      if (pos != 0)
         copy_substr(str.begin() + previous, str.begin() + pos, sstr);
      pos += whatLength;
      previous = pos;
      sstr << with;
   }

   if (previous != std::string::npos)
      copy_substr(str.begin() + previous, str.end(), sstr);

   return sstr.str();
}
}  // namespace

static llvm::cl::OptionCategory ConstifyCategory("Constify 'char*'");

bool IsInExcludeList(const std::string& path) {
   if (path.empty())
      return true;
   return std::any_of(std::begin(ExcludePaths), std::end(ExcludePaths),
                      [&path](const std::string& e) {
                         return strstr(path.c_str(), e.c_str()) != nullptr;
                      });
}


// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class ConstifyVisitor : public RecursiveASTVisitor<ConstifyVisitor> {
public:
   typedef RecursiveASTVisitor<ConstifyVisitor> base;

   ConstifyVisitor(UseDefGraph& G, NodeManager& nodes, SourceManager& SM,
                   std::vector<Replacement>&      replacements,
                   std::set<const FunctionDecl*>& lockedFunctions)
      : m_graph(G)
      , m_nodes(nodes)
      , m_sourceManager(SM)
      , m_replacements(replacements)
      , m_lockedFunctions(lockedFunctions)
      , m_current(nullptr)
      , m_tufunctions() {}

   std::string PathOf(Decl* d) const {
      if (!d)
         return "";
      auto loc = d->getLocation();
      return LocationPath(loc, m_sourceManager);
   }

   std::string PathOf(Expr* e) const {
      if (!e)
         return "";
      auto loc = e->getLocStart();
      return LocationPath(loc, m_sourceManager);
   }

   bool IsOutsideSources(Decl* d) const {
      if (!d)
         return true;
      std::string path = PathOf(d);
      return IsInExcludeList(path);
   }

   bool IsOutsideSources(Expr* e) const {
      if (!e)
         return true;
      if (m_current)
         return IsOutsideSources(m_current);
      return IsInExcludeList(PathOf(e));
   }

   bool VisitVarDecl(VarDecl* vardecl) {
      bool modifiable = !IsOutsideSources(vardecl);

      auto                 type        = vardecl->getType();
      auto                 infos       = isCharstarConst(type);
      UseDefGraph::Node_t* vardeclnode = nullptr;
      if (infos.first) {
         if (!modifiable) {
            if (Verbose) {
               llvm::raw_os_ostream ostr(std::cout);
               std::cout << "Found readonly var: ";
               vardecl->dump(ostr);
            }
            vardeclnode =
               m_graph.addNode(m_nodes.nodeof<ReadOnlyVarNode>(vardecl));
            return true;
         }
         else {
            if (Verbose) {
               std::cout << "Found var: ";
               llvm::raw_os_ostream ostr(std::cout);
               vardecl->dump(ostr);
            }
            vardeclnode = m_graph.addNode(m_nodes.nodeof<VarNode>(vardecl));
         }
      }
      else if (type->isVoidPointerType()) {
         if (Verbose) {
            std::cout << "Found readonly var: ";
            llvm::raw_os_ostream ostr(std::cout);
            vardecl->dump(ostr);
         }
         vardeclnode =
            m_graph.addNode(m_nodes.nodeof<ReadOnlyVarNode>(vardecl));
      }

      if (vardeclnode) {
         if (vardecl->hasInit())
            VisitRhs(vardecl->getInit(), vardeclnode->value(), nullptr, false);
         ReplaceForwardDeclaration(vardecl, vardeclnode);
      }
      else {
         LockRhs(vardecl->getInit());
      }
      return true;
   }

   void ReplaceForwardDeclaration(VarDecl*             vardecl,
                                  UseDefGraph::Node_t* vardeclnode) {
      auto parent = vardecl->getParentFunctionOrMethod();
      if (!parent)
         return;

      if (auto fct = dyn_cast<FunctionDecl>(parent)) {
         auto declInPrototype =
            std::find(fct->param_begin(), fct->param_end(), vardecl);
         if (declInPrototype == fct->param_end())
            return;

         if (auto previous = fct->getPreviousDecl()) {
            auto index   = std::distance(fct->param_begin(), declInPrototype);
            auto fwddecl = previous->getParamDecl(index);
            if (auto fwdnode = m_nodes.find(fwddecl)) {
               // transfert links to new node.
               if (Verbose) {
                  llvm::raw_os_ostream ostr(std::cout);
                  std::cout << "Replace forward declaration ";
                  fwddecl->dump(ostr);
                  std::cout << " to new definition ";
                  vardecl->dump(ostr);
               }
               auto n = m_graph.findNode(fwdnode);
               for (auto& prevlink : n->Predecessors())
                  m_graph.addEdge(&prevlink, vardeclnode);
               for (auto& nextlink : n->Neighbors())
                  m_graph.addEdge(vardeclnode, &nextlink);
               m_graph.delNode(fwdnode);
            }
         }
      }
   }


   bool VisitCallExpr(CallExpr* callexpr) {
      if (IsOutsideSources(callexpr))
         return true;

      if (VisitDirectCallExpr(callexpr))
         return true;

      if (VisitIndirectCallExpr(callexpr))
         return true;

      return true;
   }

   bool VisitDirectCallExpr(CallExpr* callexpr) {
      auto calleeFundecl = callexpr->getDirectCallee();
      if (!calleeFundecl)
         return false;

      Function f(calleeFundecl);
      for (std::size_t i = 0; i < f.parameters_size(); ++i) {
         auto p     = f.parameter_at(i);
         auto arg   = callexpr->getArg(i);
         auto param = calleeFundecl->getParamDecl(i);

         if (VisitCallExprValueArg(arg, param, p, callexpr))
            continue;

         if (VisitCallExprLiteralArg(arg, param, p, callexpr))
            continue;

         if (VisitCallExprPtrFunction(arg, param, p, callexpr))
            continue;
      }
      return true;
   }



   bool VisitIndirectCallExpr(CallExpr* callexpr) {
      auto calleeIndirect = callexpr->getCalleeDecl();
      if (!calleeIndirect)
         return false;

      auto functionProtoType = ExtractFunctionProto(calleeIndirect);
      if (!functionProtoType)
         return false;

      for (std::size_t i = 0; i < callexpr->getNumArgs(); ++i) {
         auto arg     = callexpr->getArg(i);
         auto argType = arg->getType();

         if (!isCharstarConst(argType).first && !argType->isVoidPointerType())
            continue;

         auto vardecls = ExtractValueDecls(arg);
         if (vardecls.empty())
            continue;

         for (auto vardecl : vardecls) {

            auto argnode = m_nodes.find(vardecl);
            if (!argnode) {
               std::cerr << "Error during parsing: argument #" << i
                         << " should be already know!\n";
               if (!Quiet)
                  callexpr->dump();
               continue;
            }

            if (i > functionProtoType->getNumParams()) {
               if (!Quiet) {
                  std::cerr << "Warning: missmatch number of arguments "
                            << "(call with " << i << " but only "
                            << functionProtoType->getNumParams() << "defined)."
                            << std::endl;
                  functionProtoType->dump();
                  std::cerr << std::endl;
               }

               continue;
            }

            // auto paramtype = functionProtoType->getParamType(i);
            auto paramnode = m_nodes.prototypeNode(functionProtoType, i);
            m_graph.addNode(paramnode);
            m_graph.addEdge(argnode, paramnode);
         }
      }
      return true;
   }

   bool VisitCallExprValueArg(Expr* arg, ParmVarDecl* paramdecl,
                              const Parameter& p, CallExpr* callexpr) {
      auto vardecls = ExtractValueDecls(arg);
      if (vardecls.empty())
         return false;

      for (auto vardecl : vardecls) {

         auto aType = isCharstarConst(vardecl->getType());
         auto pType = isCharstarConst(paramdecl->getType());

         // both are not char*
         if (!aType.first && !pType.first)
            continue;

         // both are char*
         if (aType.first && pType.first) {
            auto argnode = m_nodes.find(vardecl);
            if (!argnode) {
               std::cerr << "Warning: unknown argument \""
                         << vardecl->getNameAsString() << "\"." << std::endl;
               VisitVarDecl(vardecl);
               argnode = m_nodes.find(vardecl);
               if (!argnode) {
                  std::cerr << "Error during parsing: argument\""
                            << vardecl->getNameAsString()
                            << "\" should be already know!" << std::endl;
                  if (!Quiet)
                     callexpr->dump();
                  continue;
               }
            }

            auto paramnode = m_nodes.find(paramdecl);
            if (!paramnode) {
               if (Verbose) {
                  std::cout << "Found parameter: ";
                  llvm::raw_os_ostream ostr(std::cout);
                  paramdecl->dump(ostr);
               }
               paramnode = m_nodes.nodeof<VarNode>(paramdecl);
               m_graph.addNode(paramnode);
            }

            if (paramnode && argnode) {
               if (Verbose)
                  std::cout << "Add link: " << *argnode << " --> " << *paramnode
                            << "\n";
               m_graph.addEdge(argnode, paramnode);
            }
            continue;
         }

         // parameter is char*, argument is not.
         // impossible case!
         if (!aType.first && pType.first)
            continue;

         // argument is char*, parameter is not (void* for example)
         if (aType.first && !pType.first) {
            auto argnode = m_nodes.find(vardecl);
            if (!argnode) {
               std::cerr << "Warning: unknown argument \""
                         << vardecl->getNameAsString() << "\"." << std::endl;
               VisitVarDecl(vardecl);
               argnode = m_nodes.find(vardecl);
               if (!argnode) {
                  std::cerr << "Error during parsing: argument\""
                            << vardecl->getNameAsString()
                            << "\" should be already know!" << std::endl;
                  if (!Quiet)
                     callexpr->dump();
                  continue;
               }
            }

            auto paramnode = m_nodes.find(paramdecl);
            if (!paramnode) {
               if (Verbose) {
                  std::cout << "Found parameter: ";
                  llvm::raw_os_ostream ostr(std::cout);
                  paramdecl->dump(ostr);
               }
               paramnode = m_nodes.nodeof<VarNode>(paramdecl, false);
               m_graph.addNode(paramnode);
            }

            if (paramnode && argnode) {
               if (Verbose)
                  std::cout << "Add link: " << *argnode << " --> " << *paramnode
                            << "\n";
               m_graph.addEdge(argnode, paramnode);
            }
         }
      }

      return true;
   }

   bool VisitCallExprLiteralArg(Expr* arg, ParmVarDecl* paramdecl,
                                const Parameter& p, CallExpr* callexpr) {
      auto strliteral = ExtractLiteral(arg);
      if (!strliteral || !p.chartype)
         return false;

      auto strnode   = m_nodes.literalNode<StringLiteralNode>(strliteral);
      auto paramnode = m_nodes.find(paramdecl);
      m_graph.addNode(strnode);
      if (paramnode)
         m_graph.addEdge(paramnode, strnode);

      return true;
   }

   bool VisitCallExprPtrFunction(Expr* arg, ParmVarDecl* paramdecl,
                                 const Parameter& p, CallExpr* callexpr) {
      auto fcts = ExtractDecls<FunctionDecl>(arg);
      if (fcts.empty())
         return false;

      for (auto fct : fcts) {
         m_lockedFunctions.insert(fct);
      }
      return true;
   }

   bool VisitUnaryOperator(UnaryOperator* unaryop) {
      auto vardecl = ExtractValueDecl(unaryop->getSubExpr());
      auto dummy   = m_nodes.dummyModifier();
      m_graph.addNode(dummy);
      m_graph.addEdge(m_nodes.find(vardecl), dummy);
      return true;
   }

   bool VisitBinaryOperator(BinaryOperator* binop) {

      if (IsOutsideSources(binop))
         return true;

      if (!binop->isAssignmentOp())
         return true;

      auto lhs = binop->getLHS();

      UseDefNode* lhs_var = nullptr;

      if (auto lhs_vardecl = ExtractValueDecl(lhs)) {
         lhs_var = m_nodes.find(lhs_vardecl);
      }

      if (!lhs_var) {
         if (auto lhs_voidptrdecl = ExtractVoidPointerDecl(lhs)) {
            lhs_var = m_nodes.find(lhs_voidptrdecl);
         }
      }

      if (auto lhs_field = ExtractFieldDecl(lhs)) {
         auto fldtype = lhs_field->getType();
         if (isCharstarConst(fldtype).first || fldtype->isVoidPointerType()) {
            lhs_var = m_nodes.nodeof<FieldNode>(lhs_field);
            m_graph.addNode(lhs_var);
         }
      }

      bool isderef = IsDereferenced(lhs);

      if (lhs_var) {
         VisitRhs(binop->getRHS(), lhs_var, binop, isderef);
      }
      else {
         LockRhs(binop->getRHS());
      }

      return true;
   }

   bool IsDereferenced(Expr* e) {
      if (!e)
         return false;

      bool isderef = false;

      while (true) {
         if (ImplicitCastExpr* implicitCast = dyn_cast<ImplicitCastExpr>(e)) {
            e = implicitCast->getSubExpr();
            continue;
         }
         if (isa<ArraySubscriptExpr>(e)) {
            isderef = true;
            break;
         }
         if (ParenExpr* parenExpr = dyn_cast<ParenExpr>(e)) {
            e = parenExpr->getSubExpr();
            continue;
         }
         if (UnaryOperator* unaryOp = dyn_cast<UnaryOperator>(e)) {
            isderef = unaryOp->getOpcode() == UO_Deref;
            break;
         }
         break;
      }

      return isderef;
   }

   void VisitRhs(Expr* rhs, UseDefNode* lhs_var, BinaryOperator* binop,
                 bool isderef) {
      UseDefNode* rhs_var      = nullptr;
      auto        rhs_vardecls = ExtractValueDecls(rhs);
      if (!rhs_vardecls.empty()) {
         for (auto rhs_vardecl : rhs_vardecls) {
            rhs_var = m_nodes.find(rhs_vardecl);

            if (!lhs_var || !rhs_var) {
               std::cerr << "\nError during parsing: lhs " << lhs_var
                         << " or rhs " << rhs_var
                         << " should be already know!\n";
               if (!Quiet)
                  rhs->getLocStart().dump(m_sourceManager);
               continue;
            }

            // if dest is non const, this is an alias.
            std::string link = lhs_var->isConst() ? " --> " : " <--> ";
            if (Verbose)
               std::cout << "\nAdd link: " << *lhs_var << link << *rhs_var
                         << "\n";

            m_graph.addEdge(lhs_var, rhs_var);
            if (!lhs_var->isConst())
               m_graph.addEdge(rhs_var, lhs_var);
            // else a standard link
         }
         return;
      }
      if (auto rhs_literal = ExtractLiteral(rhs)) {
         rhs_var = m_nodes.literalNode<StringLiteralNode>(rhs_literal);
      }
      if (auto rhs_charmodifier = ExtractCharacterLiteral(rhs)) {
         // should be deref here!
         rhs_var = m_nodes.literalNode<CharacterLiteralNode>(rhs_charmodifier);
      }
      if (auto rhs_intmodifier = ExtractIntegerLiteral(rhs)) {
         // differenciate:
         //   foo = NULL;
         // from:
         //   *foo = 0;
         if (isderef)
            rhs_var = m_nodes.literalNode<IntegralLiteralNode>(rhs_intmodifier);
         else
            rhs_var =
               m_nodes.literalNode<IntegralLiteralConstNode>(rhs_intmodifier);
      }
      if (auto rhs_callexpr = ExtractCallExpr(rhs)) {
         // direct call or indirect call.
         UseDefNode* callnode = nullptr;
         if (auto directcall = rhs_callexpr->getDirectCallee()) {
            callnode = m_nodes.find(directcall);
            if (!callnode) {
               VisitFunctionDecl(directcall->getFirstDecl());
               callnode = m_nodes.find(directcall);
            }
         }
         else if (auto calleeIndirect = rhs_callexpr->getCalleeDecl()) {
            if (auto functionProtoType = ExtractFunctionProto(calleeIndirect)) {
               callnode = m_nodes.prototypeNode(functionProtoType, -1);
            }
         }
         if (!callnode) {
            std::cerr << varName(rhs_callexpr->getDirectCallee())
                      << " not found!";
            if (binop) {
               std::cerr << " in :" << std::endl;
               binop->dump();
            }
            else {
               std::cerr << std::endl;
            }
            // unknow return type (char[], ...)
            // register an empty modifier node to avoid invalid constification.
            auto dummy = m_nodes.dummyModifier();
            m_graph.addNode(dummy);
            m_graph.addEdge(lhs_var, dummy);
         }
         else {
            if (Verbose)
               std::cout << "Add link: " << *callnode << " --> " << *lhs_var
                         << "\n";
            m_graph.addEdge(callnode, lhs_var);
         }
         return;
      }

      if (rhs_var) {
         if (Verbose) {
            if (rhs_var->getDecl()) {
               std::cout << "Found rhs: ";
               llvm::raw_os_ostream ostr(std::cout);
               rhs_var->getDecl()->dump(ostr);
            }
            std::cout << "Add link: " << *lhs_var << " --> " << *rhs_var
                      << "\n";
         }
         m_graph.addNode(rhs_var);
         m_graph.addEdge(lhs_var, rhs_var);
      }
   }

   void LockRhs(Expr* rhs) {
      auto rhs_vardecls = ExtractValueDecls(rhs);
      for (auto rhs_vardecl : rhs_vardecls) {
         UseDefNode* rhs_var = m_nodes.find(rhs_vardecl);
         if (!rhs_var)
            std::cerr
               << "\nError during parsing: rhs should be already know!\n";
         if (!Quiet)
            rhs->getLocStart().dump(m_sourceManager);
         if (Verbose)
            std::cout << "\nUnknown lhs, lock rhs var: " << *rhs_var << "\n";
         auto dummy = m_nodes.dummyModifier();
         m_graph.addNode(dummy);
         m_graph.addEdge(rhs_var, dummy);
      }

      auto rhs_fctdecls = ExtractDecls<FunctionDecl>(rhs);
      for (auto fct : rhs_fctdecls) {
         m_lockedFunctions.insert(fct);
      }
   }

   template <typename T>
   struct LocalMemorize {
      explicit LocalMemorize(T& data, T newvalue)
         : slot(data)
         , value(data) {
         slot = newvalue;
      }

      ~LocalMemorize() {
         slot = value;
      }

      template <typename U>
      operator U() const {
         return static_cast<U>(value);
      }

      T& slot;
      T  value;
   };

   typedef LocalMemorize<FunctionDecl*> MemorizedFunctionDecl;


   bool TraverseFunctionDecl(FunctionDecl* fundecl) {
      MemorizedFunctionDecl memo_fct(m_current, fundecl);
      return base::TraverseFunctionDecl(fundecl);
   }


   bool VisitFunctionDecl(FunctionDecl* fundecl) {
      // Handle mfree case here.
      // mfree is overload with 4 definitions:
      //   - void mfree(void* p);
      //   - void mfree(const void* p);
      //   - void mfree(const char* p);
      //   - void mfree(const char** p);
      // but only the first should be use to be correct with free.
      // If we keep this tool to register the "const char*" version,
      // it will constify too much vardecl.

      auto type  = fundecl->getReturnType();
      auto infos = isCharstarConst(type);
      if (infos.first || type->isVoidPointerType())
         m_graph.addNode(m_nodes.nodeof<FunctionNode>(fundecl));

      // Track local function only
      if (!IsOutsideSources(fundecl))
         m_tufunctions.insert(fundecl);
      return true;
   }

   bool VisitCXXMethodDecl(CXXMethodDecl* methoddecl) {
      auto type  = methoddecl->getReturnType();
      auto infos = isCharstarConst(type);
      if (infos.first)
         m_graph.addNode(m_nodes.nodeof<FunctionNode>(methoddecl));
      return true;
   }

   bool VisitReturnStmt(ReturnStmt* returnstmt) {
      if (!m_current)
         return true;

      auto vardecls = ExtractValueDecls(returnstmt->getRetValue());
      if (vardecls.empty())
         return true;

      for (auto vardecl : vardecls) {
         auto varnode = m_nodes.find(vardecl);
         if (!varnode) {
            std::cerr << "Error during parsing: var should be already know!\n";
            if (!Quiet)
               vardecl->dump();
         }

         auto fctnode = m_nodes.find(m_current);
         if (Verbose) {
            std::cout << "Add link: " << *varnode << " --> " << *fctnode
                      << "\n";
         }
         m_graph.addEdge(varnode, fctnode);
      }

      return true;
   }

   bool VisitDeclRefExpr(DeclRefExpr* e) {

      auto fundecl = ExtractFunctionDecl(e);
      if (!fundecl)
         return true;

      if (IsOutsideSources(fundecl))
         return true;

      // auto epath = PathOf(e);
      // auto fpath = PathOf(fundecl);

      if (m_tufunctions.find(fundecl) != m_tufunctions.end()) {

         std::size_t n = 0;
         for (auto& p : fundecl->parameters()) {
            auto node = m_nodes.find(p);
            if (node) {
               auto methodnode =
                  m_nodes.functionPointerParameterNode(fundecl, n);
               m_graph.addNode(methodnode);
               m_graph.addEdge(methodnode, node);
            }
            ++n;
         }
      }
      return true;
   }

   bool VisitImplicitCastExpr(ImplicitCastExpr* e) {
      auto targetType = e->getType();

      auto sub = e->getSubExpr();
      if (RemoveUselessCast(sub, targetType))
         return true;

      if (auto Conditional = dyn_cast<ConditionalOperator>(sub)) {
         RemoveUselessCast(Conditional->getLHS(), targetType);
         RemoveUselessCast(Conditional->getRHS(), targetType);
         return true;
      }

      return true;
   }

   bool RemoveUselessCast(Expr* e, const QualType& targetType) {
      if (auto ConstCast = dyn_cast<CXXConstCastExpr>(e)) {
         RemoveUselessConstCast(ConstCast, targetType);
         return true;
      }

      if (auto CCast = dyn_cast<CStyleCastExpr>(e)) {
         RemoveUselessCCast(CCast, targetType);
         return true;
      }
      return false;
   }

   void RemoveUselessConstCast(CXXConstCastExpr* ConstCast,
                               const QualType&   targetType) {
      auto sourceType = ConstCast->getType();

      auto targetInfo = isCharstarConst(targetType);
      auto sourceInfo = isCharstarConst(sourceType);

      if (targetInfo.first && targetInfo.second && sourceInfo.first &&
          !sourceInfo.second) {
         // const char* str = const_cast<char*>(xxx);
         // Remove this const_cast
         auto startLoc  = ConstCast->getLocStart();
         auto rparenLoc = ConstCast->getRParenLoc();
         auto subExpr   = ConstCast->getSubExpr();
         auto l         = subExpr->getExprLoc();
         if (rparenLoc.isMacroID())
            return;
         m_replacements.push_back(
            Replacement(m_sourceManager,
                        CharSourceRange(SourceRange(startLoc, l), false), ""));
         m_replacements.push_back(
            Replacement(m_sourceManager, rparenLoc, 1, ""));
      }
   }

   void RemoveUselessCCast(CStyleCastExpr* CCast, const QualType& targetType) {
      auto sourceType = CCast->getType();

      auto targetInfo = isCharstarConst(targetType);
      auto sourceInfo = isCharstarConst(sourceType);

      // const char* str = (char*)(xxx); where xxx is "const char*"
      if (targetInfo.first && targetInfo.second && sourceInfo.first &&
          !sourceInfo.second) {
         // now check type of xxx

         auto subExpr     = CCast->getSubExpr();
         auto subExprInfo = isCharstarConst(subExpr->getType());
         if (subExprInfo.first && subExprInfo.second) {
            // Remove this cast
            auto lparenLoc = CCast->getLParenLoc();
            auto rparenLoc = CCast->getRParenLoc();
            if (rparenLoc.isMacroID())
               return;
            m_replacements.push_back(Replacement(
               m_sourceManager,
               CharSourceRange(SourceRange(lparenLoc, rparenLoc), false), ""));
            m_replacements.push_back(
               Replacement(m_sourceManager, rparenLoc, 1, ""));
         }
      }
   }


private:
   UseDefGraph&                   m_graph;
   NodeManager&                   m_nodes;
   SourceManager&                 m_sourceManager;
   std::vector<Replacement>&      m_replacements;
   std::set<const FunctionDecl*>& m_lockedFunctions;
   FunctionDecl*                  m_current;
   std::set<FunctionDecl*>        m_tufunctions;
};

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class ConstifyConsumer : public ASTConsumer {
public:
   ConstifyConsumer(UseDefGraph& G, NodeManager& nodes, SourceManager& SM,
                    std::vector<Replacement>&      replacements,
                    std::set<const FunctionDecl*>& lockedFunctions)
      : m_visitor(G, nodes, SM, replacements, lockedFunctions) {}

   // Override the method that gets called for each parsed top-level
   // declaration.
   bool HandleTopLevelDecl(DeclGroupRef DR) override {
      for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
         // Traverse the declaration using our AST visitor.
         // if b is functionDecl --> switch current function in visitor.
         m_visitor.TraverseDecl(*b);
         if (AstDump)
            (*b)->dump();
      }
      return true;
   }

private:
   ConstifyVisitor m_visitor;
};


// For each source file provided to the tool, a new FrontendAction is created.
class ConstifyFrontendAction : public ASTFrontendAction {
public:
   ConstifyFrontendAction()
      : m_rewriter()
      , m_graph()
      , m_entries()
      , m_extraReplacements()
      , m_lockedFunctions() {}

   void EndSourceFileAction() override {
      // Do All the rewrite here.
      // --> perform computation on graph.

      // merge assignment node together.

      SourceManager& SM = m_rewriter.getSourceMgr();
      if (Verbose || GraphDump)
         dumpGraph(SM);
      mergeAssignmentNodes();
      if (Verbose || GraphDump)
         dumpGraph(SM);
      auto replacements = computeReplacements(SM);
      writeReplacements(SM, replacements);
   }

   std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI,
                                                  StringRef file) override {
      m_graph.clear();
      if (Verbose)
         std::cerr << "============ Compute ============\n    " << file.str()
                   << "\n";
      m_rewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
      return llvm::make_unique<ConstifyConsumer>(
         m_graph, m_nodes, CI.getSourceManager(), m_extraReplacements,
         m_lockedFunctions);
   }

   class Tarjan {
   public:
      typedef std::map<const UseDefGraph::Node_t*, std::pair<int, int>>
                                                     NodeInfos;
      typedef std::stack<const UseDefGraph::Node_t*> NodeStack;

      typedef std::list<std::vector<const UseDefGraph::Node_t*>> SCCS;



      SCCS operator()(const UseDefGraph& G) {
         std::for_each(G.beginNodes(), G.endNodes(),
                       [&](const UseDefGraph::NodeIterator_t::value_type& n) {
                          infos[n] = std::make_pair(-1, -1);
                       });

         index = 0;
         std::for_each(G.beginNodes(), G.endNodes(),
                       [&](const UseDefGraph::NodeIterator_t::value_type& n) {
                          if (infos[n].first < 0)
                             strongconnect(n);
                       });
         return sccs;
      }

   private:
      void strongconnect(const UseDefGraph::Node_t* v) {
         infos[v] = std::make_pair(index, index);
         ++index;
         S.push_back(v);

         for (auto& neighbor : v->Neighbors()) {
            auto w = &neighbor;
            if (infos[w].first < 0) {
               strongconnect(w);
               infos[v].second = std::min(infos[v].second, infos[w].second);
            }
            else if (std::find(S.begin(), S.end(), w) != S.end()) {
               infos[v].second = std::min(infos[v].second, infos[w].first);
            }
         }
         if (infos[v].second == infos[v].first) {
            std::vector<const UseDefGraph::Node_t*> scc;
            const UseDefGraph::Node_t*              w;
            do {
               w = S.back();
               S.pop_back();
               scc.push_back(w);
            } while (w != v);
            sccs.push_back(scc);
         }
      }

      NodeInfos                               infos;
      int                                     index;
      std::vector<const UseDefGraph::Node_t*> S;
      SCCS                                    sccs;
   };



   std::vector<UseDefNode*> ExtractContributors(
      const std::vector<const UseDefGraph::Node_t*>& scc) {
      std::vector<UseDefNode*> contributors;
      std::transform(scc.begin(), scc.end(), std::back_inserter(contributors),
                     [](const UseDefGraph::Node_t* n) { return n->value(); });
      return contributors;
   }

   std::vector<UseDefNode*> ExtractPredecessors(
      const std::vector<const UseDefGraph::Node_t*>& scc) {
      std::vector<UseDefNode*> predecessors;
      flat_map_transform(
         scc.begin(), scc.end(), std::back_inserter(predecessors),
         [](const UseDefGraph::Node_t* n) { return n->Predecessors(); },
         [](const UseDefGraph::Node_t& x) { return x.value(); });
      return predecessors;
   }

   std::vector<UseDefNode*> ExtractNeighbors(
      const std::vector<const UseDefGraph::Node_t*>& scc) {
      std::vector<UseDefNode*> neighbors;
      flat_map_transform(
         scc.begin(), scc.end(), std::back_inserter(neighbors),
         [](const UseDefGraph::Node_t* n) { return n->Neighbors(); },
         [](const UseDefGraph::Node_t& x) { return x.value(); });
      return neighbors;
   }



   void mergeAssignmentNodes() {
      if (Verbose)
         std::cerr << "\n============= SCCs ==============\n";

      auto sccs = Tarjan()(m_graph);

      if (Verbose) {
         std::stringstream buffer;
         int               index = 0;
         for (auto scc : sccs) {
            buffer << "Scc #" << index << std::endl << "   ";
            for (auto x : scc) {
               buffer << "\"" << *(x->value()) << "\" ";
            }
            buffer << std::endl;
            ++index;
         }
         std::cerr << buffer.str();
      }

      inplace_remove_if(sccs,
                        [](const std::vector<const UseDefGraph::Node_t*>& scc) {
                           return scc.size() < 2;
                        });

      for (auto scc : sccs) {
         std::vector<UseDefNode*> contributors = ExtractContributors(scc);
         std::vector<UseDefNode*> predecessors = ExtractPredecessors(scc);
         std::vector<UseDefNode*> neighbors    = ExtractNeighbors(scc);
         predecessors.erase(except(predecessors.begin(), predecessors.end(),
                                   contributors.begin(), contributors.end()),
                            predecessors.end());
         neighbors.erase(except(neighbors.begin(), neighbors.end(),
                                contributors.begin(), contributors.end()),
                         neighbors.end());
         for (auto c : contributors)
            m_graph.delNode(c);

         auto mergednode = m_nodes.compositeNode(std::move(contributors));
         m_graph.addNode(mergednode);

         for (auto p : predecessors)
            m_graph.addEdge(p, mergednode);
         for (auto n : neighbors)
            m_graph.addEdge(mergednode, n);
      }
   }

   void dumpGraph(SourceManager& SM) {
      std::stringstream graphstr;
      graphstr << make_named_graph(
         m_graph, SM.getFileEntryForID(SM.getMainFileID())->getName());

      std::cerr << "\n============= Graph =============\n"
                << graphstr.str() << "\n";
   }

   std::vector<Replacement> computeReplacements(SourceManager& SM) {
      if (Verbose)
         std::cerr << "\n============ Actions ============\n";

      std::vector<Replacement>       replacements(m_extraReplacements);
      std::set<const FileEntry*>&    entries         = m_entries;
      std::set<const FunctionDecl*>& lockedFunctions = m_lockedFunctions;

      m_entries.insert(SM.getFileEntryForID(SM.getMainFileID()));

      TopologicalVisit(
         m_graph, [&replacements, &SM, &entries,
                   &lockedFunctions](const UseDefGraph::Node_t& n) {
            std::stringstream buffer;

            const UseDefNode* current = n.value();
            buffer << *current;

            bool isParentFunctionLocked =
               lockedFunctions.find(current->getParentFunctionDecl()) !=
               lockedFunctions.end();

            if (!current->isConst() && current->isConstifiable() &&
                !isParentFunctionLocked) {

               bool couldbeconst = true;
               for (auto& next : n.Neighbors())
                  couldbeconst &= next.value()->isConst();

               if (couldbeconst) {
                  current->constify(SM, replacements);
                  if (auto decl = current->getDecl()) {
                     auto fileid    = SM.getFileID(decl->getLocation());
                     auto fileentry = SM.getFileEntryForID(fileid);
                     entries.insert(fileentry);
                  }
                  buffer << " <-- to constify";
               }
               else {
                  buffer << " <-- stay unconst";
               }
            }
            buffer << std::endl;

            if (Verbose)
               std::cerr << buffer.str();
         });

      return replacements;
   }

   void writeReplacements(SourceManager&                  SM,
                          const std::vector<Replacement>& replacements) {

      if (!replacements.empty()) {
         if (Inplace || StdOut)
            writeInplaceReplacements(SM, replacements);
         else
            serializeReplacements(SM, replacements);
      }
   }

   void writeInplaceReplacements(SourceManager&                  SM,
                                 const std::vector<Replacement>& replacements) {
      // FIXME
      // if (!applyAllReplacements(replacements, m_rewriter)) {
      //    std::cerr << "Errors when applying replacements\n";
      //    return;
      // }

      if (Verbose)
         std::cerr << "\n============= Files =============\n";


      std::for_each(
         m_entries.begin(), m_entries.end(), [&](const FileEntry* e) {
            if (!Inplace)
               std::cerr << "   " << std::string(e->getName()) << "\n";


            std::unique_ptr<llvm::raw_ostream> FileStream(
               new llvm::raw_os_ostream(std::cout));
            if (Inplace) {
               std::error_code EC;
               FileStream.reset(new llvm::raw_fd_ostream(
                  e->getName(), EC, llvm::sys::fs::F_Text));
               if (EC) {
                  std::cerr << "Could not open " << std::string(e->getName())
                            << " for writing\n";
                  return;
               }
            }

            FileID ID = SM.translateFile(e);

            m_rewriter.getEditBuffer(ID).write(*FileStream);
            if (Verbose)
               std::cerr << "\n";
         });
   }

   void serializeReplacements(SourceManager&                  SM,
                              const std::vector<Replacement>& replacements) {
      std::string mainfilepath =
         SM.getFileEntryForID(SM.getMainFileID())->getName();

      auto filename =
         replace_all(llvm::sys::path::filename(mainfilepath).str(), ".", "_");

      std::stringstream outputPath;
      outputPath << GetOutputDir() << "/" << Prefix << "_rplt_" << filename
                 << "__" << GlobalIndex++ << ".yaml";
      std::error_code EC;
      raw_fd_ostream  ReplacementsFile(outputPath.str().c_str(), EC,
                                      llvm::sys::fs::F_None);
      if (EC) {
         std::cerr << "Error opening file: " << EC.message() << "\n";
         return;
      }

      std::stringstream context;
      context << "constification of " << mainfilepath;

      yaml::Output YAML(ReplacementsFile);
      auto         turs = BuildTURs(mainfilepath, context.str(), replacements);
      YAML << turs;
   }

   TranslationUnitReplacements BuildTURs(
      const std::string&              mainfilepath,
      const std::string&              context,
      const std::vector<Replacement>& replacements) {
      TranslationUnitReplacements TURs;
      TURs.MainSourceFile = mainfilepath;
      // TURs.Context        = context;
      TURs.Replacements.assign(replacements.begin(), replacements.end());
      return TURs;
   }



private:
   Rewriter                      m_rewriter;
   UseDefGraph                   m_graph;
   NodeManager                   m_nodes;
   std::set<const FileEntry*>    m_entries;
   std::vector<Replacement>      m_extraReplacements;
   std::set<const FunctionDecl*> m_lockedFunctions;
};



int main(int argc, const char** argv) {
   CommonOptionsParser op(argc, argv, ConstifyCategory);

   std::unique_ptr<IgnoringDiagConsumer> diagConsumer;
   if (!Quiet)
      diagConsumer.reset(new IgnoringDiagConsumer());

   ClangTool Tool(op.getCompilations(), op.getSourcePathList());
   Tool.setDiagnosticConsumer(diagConsumer.get());

   int res = Tool.run(newFrontendActionFactory<ConstifyFrontendAction>().get());
   return res;
}

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

#ifndef ENCAPSULATE_DATAMEMBER_HPP
#define ENCAPSULATE_DATAMEMBER_HPP

#include <clang/AST/Decl.h>
#include <llvm/ADT/SmallVector.h>

#include <Transform.hpp>

#include <string>

namespace tidy {

enum class CaseLevel { camel, snake };

struct EncapsulateDataMemberOptions {
   llvm::SmallVector<std::string, 4> Names;
   CaseLevel Case;
   bool      GenerateGetterSetter;
   bool      WithNonConstGetter;
};

class EncapsulateDataMember : public Transform {
public:
   EncapsulateDataMember(TransformContext*                   ctx,
                         const EncapsulateDataMemberOptions* Options);

   virtual void registerMatchers(MatchFinder* Finder) override;
   virtual void check(const MatchFinder::MatchResult& Result) override;

private:
   std::string getterName(const clang::NamedDecl* decl) const;
   std::string setterName(const clang::NamedDecl* decl) const;

private:
   const EncapsulateDataMemberOptions* Options;
};

}  // namespace tidy

#endif

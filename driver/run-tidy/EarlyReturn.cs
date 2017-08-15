﻿//
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

using System.Collections.Generic;

using Misc;

namespace RunTidy {

   class EarlyReturn : SmallTidyTransform {
      public override void Process(ClangSource src, Patch yr, Options options) {
         Process_c_file(src.file, new List<string> { CProcessorOption }, options);
      }

      public string CProcessorOption {
         get { return "--early-return"; }
      }

      public const string OptsName = "early-return";
      public const string OptsDesc = @"Tranform function' ifs to early return where possible.
Only small if statement (less than 4 inner statements) and without any return could be transform.";

      public override string OptionName {
         get { return OptsName; }
      }

      public override string OptionDesc {
         get { return OptsDesc; }
      }

   }

}

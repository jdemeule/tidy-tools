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

using System;
using System.Collections.Generic;
using System.Diagnostics;
using Misc;

namespace RunTidy {

   class ClangTidy : Transform {

      public override void Process(ClangSource src, Patch yr, Options options) {
         RunClangTidyOn(src.file, options);
      }

      const string ClangTidyExe = @"clang-tidy";

      protected void RunClangTidyOn(string sourcefile, Options options) {
         var exportName = System.IO.Path.GetFileName(sourcefile).Replace('.', '_') + "-tidy.yaml";
         var exportPath = options.OutputDir.AppendPath(exportName);
         var parameters = new List<string> {
            "-checks=\"-*," + Parameters + "\"",
            "-export-fixes=" + exportPath,
            "-p " + options.WorkingPath,
            sourcefile
         };

         Shell.Execute(ClangTidyExe,
                parameters.JoinWith(" "),
                options.WorkingPath,
                (object sender, DataReceivedEventArgs e) => {
                   if (e == null || e.Data == null || !options.Verbose)
                      return;

                   Console.Out.WriteLine(e.Data);
                });
      }

      public override string OptionName {
         get { return "clang-tidy="; }
      }

      public override string OptionDesc {
         get { return @"Run clang-tidy transformation(s).
Call 'clang-tidy -list-checks -checks=""*""' for the full list."; }
      }

   }

}

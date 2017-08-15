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
using System.Linq;
using Misc;

namespace RunTidy {

   public abstract class SmallTidyTransform : Transform {

      const string SmallTidyExe = @"small-tidy";

      protected void Process_c_file(string sourcefile, IEnumerable<string> args, Options options) {

         var parameters = new List<string> {
            (options.Verbose ? "" : "--quiet"),
            "--outputdir=" + options.OutputDir,
            "-export",
            "-p " + options.WorkingPath,
            sourcefile
         };

         ProcessSource(args.Union(parameters).JoinWith(" "), options);
      }

      public int ProcessSource(string parameters, Options options) {
         if (options.Verbose)
            Console.Out.WriteLine("{0} {1}", SmallTidyExe, parameters);
         Shell.Execute(SmallTidyExe,
                       parameters,
                       Environment.CurrentDirectory,
                       new DataReceivedEventHandler(p_OutputDataReceived));
         return 0;
      }

      void p_OutputDataReceived(object sender, DataReceivedEventArgs e) {
         if (e == null || e.Data == null)
            return;

         Console.Out.WriteLine(e.Data);
      }

   }

}

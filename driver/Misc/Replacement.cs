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

using System.Collections.Generic;
using System.IO;

namespace Misc {

   public class Replacement {
      public string FilePath { get; set; }
      public int Offset { get; set; }
      public int Length { get; set; }
      public string ReplacementText { get; set; }
   }

   public class Patch {
      public string MainSourceFile { get; set; }
      public List<Replacement> Replacements { get; set; }
   }

   public class ClangPatchFormatter {

      public void Write(Patch patch, TextWriter output) {
         output.WriteLine("---");
         output.WriteLine("MainSourceFile:  '{0}'", patch.MainSourceFile.PosixPath());
         output.WriteLine("Context:         'constification of {0}'", patch.MainSourceFile.PosixPath());
         output.WriteLine("Replacements:");
         foreach (var r in patch.Replacements) {
            output.WriteLine("  - FilePath:        '{0}'", r.FilePath.PosixPath());
            output.WriteLine("    Offset:          {0}", r.Offset);
            output.WriteLine("    Length:          {0}", r.Length);
            output.WriteLine("    ReplacementText: '{0}'", r.ReplacementText);
         }
         output.WriteLine("...");
      }

   }

}

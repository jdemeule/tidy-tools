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

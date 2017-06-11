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

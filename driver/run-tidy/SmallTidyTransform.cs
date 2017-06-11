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

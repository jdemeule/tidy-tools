using System;
using System.Diagnostics;
using System.IO;

namespace Misc {

   public static class Shell {

      static Process shellp(string filename, string arguments, string workingdirectory,
                            DataReceivedEventHandler dataHandler) {
         var p = new Process();
         p.StartInfo.Arguments = arguments;
         p.StartInfo.CreateNoWindow = true;

         p.StartInfo.ErrorDialog = false;
         p.StartInfo.UseShellExecute = false;
         p.StartInfo.RedirectStandardOutput = true;
         p.StartInfo.RedirectStandardInput = false;
         p.StartInfo.RedirectStandardError = true;

         p.StartInfo.FileName = filename;
         p.StartInfo.WorkingDirectory = workingdirectory;

         p.OutputDataReceived += dataHandler;
         p.ErrorDataReceived += dataHandler;
         p.Exited += p_Exited;
         return p;
      }

      static void p_Exited(object sender, EventArgs e) {

      }

      public static bool Execute(string filename, string arguments, string workingdirectory,
                                 DataReceivedEventHandler dataHandler) {
         if (!Directory.Exists(workingdirectory))
            return false;

         try {
            Process p = shellp(filename, arguments, workingdirectory, dataHandler);
            p.Start();
            p.BeginOutputReadLine();
            p.BeginErrorReadLine();
            p.WaitForExit();
            return p.ExitCode == 0;
         } catch (Exception e) {
            Console.Error.WriteLine("Canno't execute '{0} {1}'", filename, arguments);
            throw e;
         }
      }

   }

}

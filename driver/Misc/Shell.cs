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

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
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;


namespace Misc {


   public delegate void PatcherHandler(IEnumerable<string> filepaths);

   public class Patcher {

      public Patcher(string workingpath) {
         m_workingpath = workingpath;
      }

      public void ApplyPatch(string PatchFolder) {
         var patches = Directory.GetFiles(PatchFolder, "*.yaml");
         if (!patches.Any())
            return;

         var editedFiles = FilesToPatch(patches);

         BeforeApplyPatch(editedFiles);

         if (ClangApplyReplacements(PatchFolder)) {
            CleanupFolder(patches);
            AfterApplyPatch(editedFiles);
         }
      }

      static void CleanupFolder(IEnumerable<string> patches) {
         foreach (var f in patches)
            File.Delete(f);
      }

      IEnumerable<string> FilesToPatch(IEnumerable<string> patches) {
         var filepaths = new List<string>();
         var ReFilePath = new Regex(@"  - FilePath:        '(?<filepath>.*)'");
         foreach (var patch in patches) {
            foreach (var line in File.ReadAllLines(patch)) {
               Match m = ReFilePath.Match(line);
               if (m.Success)
                  filepaths.Add(m.Groups["filepath"].Value);
            }
         }
         return filepaths.ToList();
      }

      void BeforeApplyPatch(IEnumerable<string> editedFiles) {
         if (BeforeApplyingPatches == null)
            return;
         BeforeApplyingPatches(editedFiles);
      }

      void AfterApplyPatch(IEnumerable<string> editedFiles) {
         if (AfterAppliedPatches == null)
            return;
         AfterAppliedPatches(editedFiles);
      }

      const string ClangApplyReplacementsExe = @"clang-apply-replacements";

      bool ClangApplyReplacements(string OutputFolder) {
         return Shell.Execute(ClangApplyReplacementsExe,
            new List<string> {
               "-format",
               "-style=file",
               string.Format("-style-config={0}", m_workingpath),
               //"-remove-change-desc-files",
               OutputFolder
            }.JoinWith(" "),
            m_workingpath,
            new DataReceivedEventHandler(p_OutputDataReceived));
      }

      void p_OutputDataReceived(object sender, DataReceivedEventArgs e) {
         if (e == null || e.Data == null)
            return;

         Console.Out.WriteLine(e.Data);
      }

      public event PatcherHandler BeforeApplyingPatches;
      public event PatcherHandler AfterAppliedPatches;

      string m_workingpath;
   }
}

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

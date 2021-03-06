﻿//
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
using System.Text.RegularExpressions;

namespace Misc {

   public class Git {

      const string git_exe = "git";

      public Git(string workingfolder) {
         m_workingfolder = workingfolder;
      }

      public void Add(string path) {
         GitCmd(string.Format("add {0}", path));
      }

      public void Add(IEnumerable<string> paths) {
         foreach (var path in paths)
            Add(path);
      }

      public void Revert(string path) {
         GitCmd(string.Format("checkout {0}", path));
      }

      public void Revert(IEnumerable<string> paths) {
         foreach (var path in paths)
            Revert(path);
      }

      public void Branch(string name) {
         GitCmd(string.Format("checkout -b {0}", name));
      }

      public void Commit(string message) {
         GitCmd(string.Format("commit -m \"{0}\"", message));
      }

      public void Checkout(string branch) {
         GitCmd(string.Format("checkout {0}", branch));
      }

      public string CurrentBranch() {
         string name = "master";
         GitCmd("rev-parse --abbrev-ref HEAD", (object sender, DataReceivedEventArgs e) => {
            if (!string.IsNullOrEmpty(e.Data))
               name = e.Data.Trim();
         });
         return name;
      }

      public void Tag(string label) {
         GitCmd(string.Format("tag {0}", label));
      }

      public IEnumerable<string> EditedFiles(string otherbranch) {
         var re = new Regex(@"(?<operation>[MDA])\s+(?<file>.*)");

         string arg = string.IsNullOrEmpty(otherbranch)
                            ? "diff --name-status"
                            : string.Format("diff --name-status {0}", otherbranch);

         var files = new List<string>();
         GitCmd(arg, (object sender, DataReceivedEventArgs e) => {
            if (string.IsNullOrEmpty(e.Data))
               return;
            var m = re.Match(e.Data);
            if (!m.Success)
               return;
            files.Add(m.Groups["file"].Value);
         });
         return files;
      }

      public void StashSave() {
         GitCmd("stash save");
      }

      public void StashPop() {
         GitCmd("stash pop");
      }

      void GitCmd(string arguments) {
         GitCmd(arguments, new DataReceivedEventHandler(p_OutputDataReceived));
      }

      void GitCmd(string arguments, DataReceivedEventHandler dreh) {
         Shell.Execute(git_exe, arguments, m_workingfolder, dreh);
      }

      void p_OutputDataReceived(object sender, DataReceivedEventArgs e) {
         if (e == null || e.Data == null)
            return;

         Console.Out.WriteLine(e.Data);
      }

      string m_workingfolder;

   }

}

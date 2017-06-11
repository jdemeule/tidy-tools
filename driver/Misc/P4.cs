using System;
using System.Diagnostics;

namespace Misc {

   public class P4 {

      public P4(string workingfolder) {
         m_workingfolder = workingfolder;
      }

      public void Edit(string filepath) {
         Shell.Execute("p4",
                       string.Format("edit {0}", filepath),
                       m_workingfolder,
                       new DataReceivedEventHandler(p_OutputDataReceived));
      }

      public void Submit(string changelist) {
         Shell.Execute(@"p4",
                       string.Format("submit -c {0}", changelist),
                       m_workingfolder,
                       p_OutputDataReceived);
      }

      void p_OutputDataReceived(object sender, DataReceivedEventArgs e) {
         if (e == null || e.Data == null)
            return;

         Console.Out.WriteLine(e.Data);
      }

      string m_workingfolder;
   }

}

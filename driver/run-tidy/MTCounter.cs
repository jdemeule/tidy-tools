using System.Collections.Generic;
using System.IO;

namespace RunTidy {

   class MTCounter {

      public MTCounter(int top, TextWriter output, bool quiet) {
         Max = top;
         Count = 0;
         m_quiet = quiet;
         m_out = output;
         if (Max > 10) {
            m_steps.Add(25 * Max / 100);
            m_steps.Add(50 * Max / 100);
            m_steps.Add(75 * Max / 100);
            m_steps.Add(Max);
         }
      }

      public void Next() {
         lock (Mutex) {
            ++Count;
            if (!m_quiet)
               m_out.Write("\r{0}% ({1}/{2})", Count * 100 / Max, Count, Max);
         }
      }

      internal int Max;
      internal int Count;

      bool m_quiet = false;
      TextWriter m_out;
      List<int> m_steps = new List<int>();

      object Mutex = new object();
   }

}

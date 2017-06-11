using System.Threading;

namespace Misc {

   public class CountdownLatch {

      int m_remain;
      EventWaitHandle m_event;

      public CountdownLatch(int count) {
         m_remain = count;
         m_event = new ManualResetEvent(false);
      }

      public void Signal() {
         // The last thread to signal also sets the event.
         if (Interlocked.Decrement(ref m_remain) == 0)
            m_event.Set();
      }

      public void Wait() {
         m_event.WaitOne();
      }

   }

}

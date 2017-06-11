using System;
using System.Collections.Generic;

namespace Misc {

   public class Time {
      public Time() {
         m_time = 0;
      }
      public Time(int tm) {
         m_time = tm;
      }
      public int m_time;
   }

   public class TimeVar {
      public TimeVar(string name) {
         m_start = new Time();
         m_elapsed = new Time();
         m_name = name;
         m_count = 0;
      }

      public TimeVar(string name, Time elapsed) : this(name) {
         m_elapsed = elapsed;
         m_count = 1;
      }


      public void start() {
         m_start.m_time = Environment.TickCount;
         ++m_count;
      }

      public void stop() {
         m_elapsed.m_time += Environment.TickCount - m_start.m_time;
      }

      public bool isUse() {
         return m_count != 0;
      }

      public void report() {
         Console.Write("  {0}: ", m_name);
         for (int i = 0; i < (25 - m_name.Length); ++i)
            Console.Write(" ");
         var sec = (int)((float)m_elapsed.m_time / 1000);
         var min = sec / 60;
         var rsec = sec % 60;
         if (min > 0)
            Console.Write("{0,5}min {1,3}s\t", min, rsec);
         else
            Console.Write("         {0,3}s\t", sec);
         if (m_count > 1)
            Console.Write("{0} times\t {1}s in moyen", m_count, (float)m_elapsed.m_time / (1000 * m_count));
         Console.WriteLine();
      }

      public void Merge(TimeVar tm) {
         m_elapsed.m_time += tm.m_elapsed.m_time;
         m_count += tm.m_count;
      }

      public string Name {
         get { return m_name; }
      }

      Time m_start;
      Time m_elapsed;
      string m_name;
      long m_count;
   }


   public class Profiler {

      static Profiler m_instance = new Profiler();

      public static Profiler Instance {
         get {
            return m_instance;
         }
      }

      Profiler() {
         m_timeVarMap = new Dictionary<string, TimeVar>();
         m_listTimerName = new List<string>();
      }

      public void AddTimer(string name) {
         m_timeVarMap[name] = new TimeVar(name);
         m_listTimerName.Add(name);
      }

      public void RemoveTimer(string name) {
         TimeVar t = m_timeVarMap[name];
         if (t != null)
            m_timeVarMap.Remove(name);
         m_listTimerName.Remove(name);
      }

      public bool ContainsTimer(string name) {
         return m_timeVarMap.ContainsKey(name);
      }

      public void AppendTimeVar(TimeVar tv) {
         lock (m_mutex) {
            if (!ContainsTimer(tv.Name))
               AddTimer(tv.Name);
            TimeVar t = m_timeVarMap[tv.Name];
            t.Merge(tv);
         }
      }


      public void Start(string name) {
         TimeVar t = m_timeVarMap[name];
         if (t == null)
            return;
         t.start();
      }

      public void Stop(string name) {
         TimeVar t = m_timeVarMap[name];
         if (t == null)
            return;
         t.stop();
      }

      public void Clear() {
         m_timeVarMap.Clear();
         m_listTimerName.Clear();
      }

      public void Report() {
         Console.WriteLine("\nTime Report:");
         foreach (string timername in m_listTimerName) {
            TimeVar t = m_timeVarMap[timername];
            if ((t != null) && t.isUse())
               t.report();
         }
      }

      //private Hashtable _timeVarMap; // key = string, value = timeVar
      Dictionary<string, TimeVar> m_timeVarMap;
      List<string> m_listTimerName;
      static object m_mutex = new object();
   }

   public class ScopeProfiler : IDisposable {

      public ScopeProfiler(string name) {
         m_name = name;
         if (!Profiler.Instance.ContainsTimer(name))
            Profiler.Instance.AddTimer(name);
         Profiler.Instance.Start(name);
      }

      #region IDisposable Members

      public void Dispose() {
         Profiler.Instance.Stop(m_name);
      }

      #endregion

      readonly string m_name;
   }

   public class ScopeProfilerMt : IDisposable {

      public ScopeProfilerMt(string name) {
         m_name = name;
         //if (!Profiler.Instance.ContainsTimer(name))
         //   Profiler.Instance.AddTimer(name);
         //Profiler.Instance.Start(name);
         m_tick = Environment.TickCount;
      }

      #region IDisposable Members

      public void Dispose() {
         //Profiler.Instance.Stop(m_name);
         m_tm = new Time(Environment.TickCount - m_tick);
         Profiler.Instance.AppendTimeVar(new TimeVar(m_name, m_tm));
      }

      #endregion

      string m_name;
      int m_tick;
      Time m_tm;
   }


}

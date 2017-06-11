using System.Collections.Generic;

namespace RunTidy {

   public class Options {
      public bool PrintHelp { get; set; }
      public int Jobs { get; set; }
      public string OutputDir { get; set; }
      public string WorkingPath { get; set; }
      public bool ApplyPatch { get; set; }
      public bool P4Edit { get; set; }
      public bool ApplyUntilDone { get; set; }
      public bool Verbose { get; set; }
      public bool Quiet { get; set; }


      List<Transform> m_transformers = new List<Transform>();

      public List<Transform> Transformers {
         get { return m_transformers; }
         set { m_transformers = value; }
      }
   }

}

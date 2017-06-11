using Misc;

namespace RunTidy {

   public abstract class Transform {

      public abstract void Process(ClangSource src, Patch yr, Options options);

      public abstract string OptionName { get; }
      public abstract string OptionDesc { get; }

      public string Parameters { get; set; }

   }

}

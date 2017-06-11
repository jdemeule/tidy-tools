using System.Collections.Generic;
using Misc;

namespace RunTidy {

   public class SampleTransform : SmallTidyTransform {

      public override void Process(ClangSource src, Patch yr, Options options) {
         Process_c_file(src.file, new List<string> { "-" + OptsName }, options);
      }


      public const string OptsName = "sample";
      public const string OptsDesc = "Sample transform";

      public override string OptionDesc {
         get { return OptsDesc; }
      }

      public override string OptionName {
         get { return OptsName; }
      }

   }

}

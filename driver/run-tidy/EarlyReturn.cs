using System.Collections.Generic;

using Misc;

namespace RunTidy {

   class EarlyReturn : SmallTidyTransform {
      public override void Process(ClangSource src, Patch yr, Options options) {
         Process_c_file(src.file, new List<string> { CProcessorOption }, options);
      }

      public string CProcessorOption {
         get { return "--early-return"; }
      }

      public const string OptsName = "early-return";
      public const string OptsDesc = @"Tranform function' ifs to early return where possible.
Only small if statement (less than 4 inner statements) and without any return could be transform.";

      public override string OptionName {
         get { return OptsName; }
      }

      public override string OptionDesc {
         get { return OptsDesc; }
      }

   }

}

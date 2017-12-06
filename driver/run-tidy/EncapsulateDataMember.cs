using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Misc;

namespace RunTidy {

   class EncapsulateDataMember : Transform {
      public override void Process(ClangSource src, Misc.Patch yr, Options options) {
         RunOn(src.file, options);
      }

      protected void RunOn(string sourcefile, Options options) {
         var parameters = new List<string> {
            "-names=\"" + Parameters + "\"",
            "-outputdir=" + options.OutputDir,
            "-p=" + options.WorkingPath,
            sourcefile
         };

         Shell.Execute(EncapsulateDataMemberExe,
                parameters.JoinWith(" "),
                options.WorkingPath,
                (object sender, DataReceivedEventArgs e) => {
                   if (e == null || e.Data == null || !options.Verbose)
                      return;
                   Console.Out.WriteLine(e.Data);
                });
      }

      const string EncapsulateDataMemberExe = @"encapsulate-datamember";

      public override string OptionName {
         get { return "encapsulate-datamember="; }
      }

      public override string OptionDesc {
         get {
            return @"Encapsulate datamember by getter/setter and replace usage.
VALUE is the list of fully qualified datamember to encapsulate, e.g. ""Foo::xxx,a::Bar::yyy""";
         }
      }
   }
}

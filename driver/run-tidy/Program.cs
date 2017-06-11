using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Misc;
using NDesk.Options;

namespace RunTidy {

   class Program {
      static int Main(string[] args) {

         var wp = Environment.CurrentDirectory.PosixPath();

         var opts = new Options {
            WorkingPath = wp,
            Jobs = (int)(Environment.ProcessorCount * 1.25),
            OutputDir = wp.AppendPath("patches")
         };

         var optionSet = BuildOptionSet(opts);

         var transforms = new Transforms();
         transforms.FillOptionSet(optionSet, opts);

         var extras = optionSet.Parse(args);
         if (extras.Any(x => x.StartsWith("-"))) {
            Console.Error.WriteLine("Unrecognized '{0}' options", extras.Find(x => x.StartsWith("-")));
            Usage(optionSet);
            return 1;
         }

         if (opts.PrintHelp) {
            Usage(optionSet);
            return 0;
         }

         if (!Directory.Exists(opts.OutputDir))
            Directory.CreateDirectory(opts.OutputDir);

         if (!File.Exists(opts.WorkingPath.AppendPath("compile_commands.json"))) {
            Console.Error.WriteLine("'compile_commands.json' is missing, cannot process anything.");
            return 1;
         }

         var patcher = CreatePatcher(opts);

         var tidy = new RunTidy(patcher, opts);
         tidy.ProcessAll(extras);
         return 0;
      }

      static Patcher CreatePatcher(Options opts) {
         var patcher = new Patcher(opts.WorkingPath);
         if (opts.P4Edit) {
            var p4 = new P4(opts.WorkingPath);
            patcher.BeforeApplyingPatches += (filepaths) => {
               foreach (var f in filepaths)
                  p4.Edit(f);
            };
         }

         return patcher;
      }

      static OptionSet BuildOptionSet(Options opts) {
         return new OptionSet {
            {
               "h|help",
               "show this help message",
               v => { opts.PrintHelp = true; }
            },
            {
               "j|jobs=",
               "number of concurent jobs",
               (int v) => { opts.Jobs = v; }
            },
            {
               "outputdir=",
               "patches output directory",
               v => { opts.OutputDir = v; }
            },
            {
               "apply|fix",
               "apply patches",
               v => { opts.ApplyPatch = true; }
            },
            {
               "apply-until-done",
               "apply until transformations do nothing",
               v => { opts.ApplyUntilDone = true; opts.ApplyPatch = true; }
            },
            {
               "p4-edit",
               "call p4 edit before applying patches",
               v => { opts.P4Edit = true; }
            },
            {
               "debug-me",
               "call debugger on run-tidy",
               v => { Debugger.Break(); }
            },
            {
               "verbose",
               "be more verbose.",
               v => { opts.Verbose = true; }
            },
            {
               "quiet",
               "be quiet.",
               v => { opts.Quiet = true; }
            }
         };
      }

      static void Usage(OptionSet optionSet) {
         var msg = @"Usage: run-tidy [options] [files [files...]]

Runs transformation (clang-tidy, small-tidy, internal ones) over all files in a
compilation database.
Requires clang-tidy, clang-apply-replacements and small-tidy in $PATH.

Version: {0}

Positional arguments:
  files               files to be process (anything that will match within
                      compilation database contents)

Optionnal arguments:";;
         Console.Out.WriteLine(msg, VERSION);

         optionSet.WriteOptionDescriptions(Console.Out);
      }

      const string VERSION = "1d";
   }
}

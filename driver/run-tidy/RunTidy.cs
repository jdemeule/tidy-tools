using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Misc;
using Newtonsoft.Json;

namespace RunTidy {

   public class RunTidy {

      public RunTidy(Patcher patcher, Options opts) {
         m_opts = opts;
         m_patcher = patcher;
      }


      internal void ProcessAll(IEnumerable<string> filefilter) {
         var filtered = FindSourcesToProcess(filefilter);

         for (bool continueWork = true; continueWork;) {
            bool haveToContinue = false;
            foreach (var transformer in m_opts.Transformers) {
               Process(transformer, filtered);
               haveToContinue |= ApplyPatches();
            }
            continueWork = haveToContinue;
         }
      }

      List<ClangSource> FindSourcesToProcess(IEnumerable<string> filefilter) {
         var sources = new List<ClangSource>();

         using (var s = new StreamReader(m_opts.WorkingPath.AppendPath("compile_commands.json")))
         using (var r = new JsonTextReader(s)) {
            var serializer = new JsonSerializer();
            sources.AddRange(serializer.Deserialize<List<ClangSource>>(r));
         }

         var filtered = sources;

         if (filefilter.Any()) {
            var filter = new HashSet<string>(filefilter
               .Where(f => f.StartsWith("@"))
               .SelectMany(f => File.ReadAllLines(f.Substring(1)))
               .Union(filefilter.Where(f => !f.StartsWith("@")))
               .Select(f => f.PosixPath()));

            var xs = from s in sources
                     from f in filter
                     where s.file.PosixPath().Contains(f)
                     select s;

            filtered = xs.ToList();
         }
         return filtered;
      }

      bool ApplyPatches() {
         bool continueWork = Directory.GetFiles(m_opts.OutputDir).Length != 0 && m_opts.ApplyUntilDone;
         if (m_opts.ApplyPatch) {
            m_patcher.ApplyPatch(m_opts.OutputDir);
         }
         return continueWork;
      }

      void Process(Transform transformer, List<ClangSource> filtered) {
         Console.WriteLine("{0}...", transformer.OptionName);
         var counter = new MTCounter(filtered.Count, Console.Out, m_opts.Quiet);
         Parallel.ForAllPooled(filtered,
            s => {
               Process(transformer, s);
               counter.Next();
            },
            m_opts.Jobs);
         Console.Out.WriteLine("\r\ndone");
      }


      void Process(Transform transformer, ClangSource source) {
         try {
            var yr = new Patch {
               MainSourceFile = source.file,
               Replacements = new List<Replacement>()
            };

            transformer.Process(source, yr, m_opts);
            WritePatch(yr);
         } catch (Exception e) {
            Console.Error.WriteLine("Cannot process: '{0}'", source.file);
            Console.Error.WriteLine(e);
         }
      }


      void WritePatch(Patch yr) {
         if (yr.Replacements.Count == 0)
            return;

         int index = 0;
         lock (m_mutex) {
            index = m_counter++;
         }

         var filename = string.Format("{0}_{1}", index, Path.GetFileNameWithoutExtension(yr.MainSourceFile));

         using (var stream = new StreamWriter(m_opts.OutputDir.AppendPath(filename + ".yaml"))) {
            m_patchSerializer.Write(yr, stream);
         }
      }

      Options m_opts;
      ClangPatchFormatter m_patchSerializer = new ClangPatchFormatter();
      Patcher m_patcher;

      int m_counter = 0;
      object m_mutex = new object();

   }
}

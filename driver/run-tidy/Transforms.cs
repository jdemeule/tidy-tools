using System;
using System.Collections.Generic;
using System.Linq;
using NDesk.Options;

namespace RunTidy {

   class Transforms {

      public Transforms() {
         var tranformers = from type in GetType().Assembly.GetTypes()
                           where type.IsSubclassOf(typeof(Transform)) && !type.IsAbstract
                           let t = (Transform)type.GetConstructor(new Type[0]).Invoke(new object[0])
                           orderby t.OptionName
                           select t;

         m_transforms = tranformers.ToList();
         m_transformMap = m_transforms.ToDictionary(t => t.OptionName, t => t);
      }

      public void FillOptionSet(OptionSet optionSet, Options opts) {
         foreach (var t in m_transforms) {
            optionSet.Add(t.OptionName, t.OptionDesc,
               v => {
                  if (t.OptionName.EndsWith("="))
                     t.Parameters = v;
                  opts.Transformers.Add(t);
               });
         }
      }

      List<Transform> m_transforms;
      public Dictionary<string, Transform> m_transformMap;
   }

}

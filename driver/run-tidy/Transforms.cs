//
// MIT License
//
// Copyright (c) 2017 Jeremy Demeule
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

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

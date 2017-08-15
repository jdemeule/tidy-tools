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

using System.Collections.Generic;
using System.IO;

namespace RunTidy {

   class MTCounter {

      public MTCounter(int top, TextWriter output, bool quiet) {
         Max = top;
         Count = 0;
         m_quiet = quiet;
         m_out = output;
         if (Max > 10) {
            m_steps.Add(25 * Max / 100);
            m_steps.Add(50 * Max / 100);
            m_steps.Add(75 * Max / 100);
            m_steps.Add(Max);
         }
      }

      public void Next() {
         lock (Mutex) {
            ++Count;
            if (!m_quiet)
               m_out.Write("\r{0}% ({1}/{2})", Count * 100 / Max, Count, Max);
         }
      }

      internal int Max;
      internal int Count;

      bool m_quiet = false;
      TextWriter m_out;
      List<int> m_steps = new List<int>();

      object Mutex = new object();
   }

}

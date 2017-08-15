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
using System.Threading;

namespace Misc {

   public static class Parallel {

      public static void ForAll(int from, int to, Action<int> a) {
         ForAll<int>(null, from, to, null, a, Environment.ProcessorCount);
      }

      public static void ForAll(int from, int to, Action<int> a, int p) {
         ForAll<int>(null, from, to, null, a, p);
      }

      public static void ForAll<T>(IList<T> data, Action<T> a) {
         ForAll(data, 0, data.Count, a, null, Environment.ProcessorCount);
      }

      public static void ForAll<T>(IList<T> data, Action<T> a, int p) {
         ForAll(data, 0, data.Count, a, null, p);
      }

      static void ForAll<T>(IList<T> data, int from, int to, Action<T> a0, Action<int> a1, int p) {
         int size = to - from;
         int stride = (size + p - 1) / p;
         var latch = new CountdownLatch(p);

         for (int i = 0; i < p; i++) {
            int idx = i;
            ThreadPool.QueueUserWorkItem(delegate {
               int end = Math.Min(size, stride * (idx + 1));
               for (int j = stride * idx; j < end; j++) {
                  if (data != null) a0(data[j]);
                  else a1(j);
               }

               latch.Signal();
            });
         }

         latch.Wait();
      }

      public static void ForAllPooled<T>(IEnumerable<T> data, Action<T> a) {
         ForAllPooled(data, a, Environment.ProcessorCount);
      }

      public static void ForAllPooled<T>(IEnumerable<T> data, Action<T> a, int p) {
         // create a pool of worker thread
         var mutex = new object();
         var latch = new CountdownLatch(p);
         var q = new Queue<T>(data);

         for (int i = 0; i < p; i++) {
            int idx = i;
            ThreadPool.QueueUserWorkItem(delegate {

               bool done = false;

               while (!done) {
                  T j = default(T);
                  lock (mutex) {
                     if (q.Count > 0)
                        j = q.Dequeue();
                     else
                        done = true;
                  }
                  if (j != null)
                     a(j);
               }

               latch.Signal();
            });
         }

         latch.Wait();

      }

   }

}

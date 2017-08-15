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
using System.IO;

namespace Misc {

   public static class Extensions {

      public static string CombinePath(params string[] paths) {
         return string.Join(Path.DirectorySeparatorChar.ToString(), paths);
      }

      public static string AppendPath(this string basePath, string appendedPath) {
         return Path.Combine(basePath, appendedPath);
      }

      public static string WindowsPath(this string path) {
         return path.Replace('/', Path.DirectorySeparatorChar);
      }

      public static string PosixPath(this string path) {
         if (Environment.OSVersion.Platform == PlatformID.Win32NT && path.Length > 2 && path[1] == ':')
            return path.Substring(0, 2).ToLower() + path.Substring(2).Replace('\\', '/');
         return path.Replace('\\', '/');
      }

      public static IList<string> SplitPath(this string path) {
         return path.PosixPath().Split(new char[] { '/' }, StringSplitOptions.RemoveEmptyEntries);
      }

      public static IList<string> SplitBy(this string str, params char[] separators) {
         return str.Split(separators.ToArray(), StringSplitOptions.RemoveEmptyEntries);
      }

      public static string JoinWith(this IEnumerable<string> strs, string separator) {
         return string.Join(separator, strs.ToArray());
      }

      public static string CombinePath(this IEnumerable<string> paths) {
         return string.Join("/", paths.ToArray());
      }

      public static string ReducePath(this string path) {
         var folders = path.SplitPath();
         if (folders.Count <= 1)
            return path;

         var keeps = new Stack<string>();
         foreach (var f in folders) {
            if (f == "..")
               keeps.Pop();
            else
               keeps.Push(f);
         }

         return keeps.Reverse().CombinePath();
      }


      public static string Reverse(this string str) {
         var array = str.ToCharArray();
         Array.Reverse(array);
         return new string(array);
      }

      public static void AddAll<T>(this HashSet<T> set, IEnumerable<T> values) {
         foreach (var v in values)
            set.Add(v);
      }

   }

}

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

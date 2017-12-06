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
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#ifndef MISC_HPP
#define MISC_HPP

#include <iterator>
#include <sstream>
#include <string>

#if defined(__clang__)

#if __has_feature(cxx_noexcept)
#define HAS_NOEXCEPT
#endif

#elif defined(_MSC_VER)

#if _MSC_VER > 1800
#define HAS_NOEXCEPT
#endif
#else

#endif

#if defined(HAS_NOEXCEPT)
#define NOEXCEPT noexcept
#define NOEXCEPT_(x) noexcept(x)
#else
#define NOEXCEPT throw()
#define NOEXCEPT_(x)
#endif


namespace tidy {

inline void copy_substr(const std::string::const_iterator& start,
                        const std::string::const_iterator& end,
                        std::ostream&                      ostr) {
   for (std::string::const_iterator it = start; it != end; ++it)
      ostr << *it;
}

inline std::string replace_all(const std::string& str, const std::string& what,
                               const std::string& with) {
   std::stringstream      sstr;
   std::string::size_type pos        = 0;
   std::string::size_type previous   = 0;
   std::string::size_type whatLength = what.length();

   while ((pos = str.find(what, pos)) != std::string::npos) {
      if (pos != 0)
         copy_substr(str.begin() + previous, str.begin() + pos, sstr);
      pos += whatLength;
      previous = pos;
      sstr << with;
   }

   if (previous != std::string::npos)
      copy_substr(str.begin() + previous, str.end(), sstr);

   return sstr.str();
}

template <class Delim, class CharT = char,
          class Traits = std::char_traits<CharT>>
class ostream_joiner {
public:
   typedef CharT                                      char_type;
   typedef Traits                                     traits_type;
   typedef std::basic_ostream<char_type, traits_type> ostream_type;
   typedef std::output_iterator_tag                   iterator_category;
   typedef void                                       value_type;
   typedef void                                       difference_type;
   typedef void                                       pointer;
   typedef void                                       reference;

   ostream_joiner(ostream_type& os, Delim&& d)
      : output(std::addressof(os))
      , delim(std::move(d))
      , first(true) {}

   ostream_joiner(ostream_type& os, const Delim& d)
      : output(std::addressof(os))
      , delim(d)
      , first(true) {}


   template <typename T>
   ostream_joiner& operator=(const T& v) {
      if (!first)
         *output << delim;
      first = false;
      *output << v;
      return *this;
   }

   ostream_joiner& operator*() NOEXCEPT {
      return *this;
   }
   ostream_joiner& operator++() NOEXCEPT {
      return *this;
   }
   ostream_joiner& operator++(int)NOEXCEPT {
      return *this;
   }

private:
   ostream_type* output;
   Delim         delim;
   bool          first;
};


template <class CharT, class Traits, class Delim>
ostream_joiner<typename std::decay<Delim>::type, CharT, Traits>
make_ostream_joiner(std::basic_ostream<CharT, Traits>& os, Delim&& d) {
   return ostream_joiner<typename std::decay<Delim>::type, CharT, Traits>(
      os, std::forward<Delim>(d));
}
}  // namespace tidy

#endif

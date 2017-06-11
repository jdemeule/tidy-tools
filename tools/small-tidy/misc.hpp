#ifndef MISC_HPP
#define MISC_HPP

#include <iterator>
#include <sstream>
#include <string>

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

   ostream_joiner& operator*() noexcept {
      return *this;
   }
   ostream_joiner& operator++() noexcept {
      return *this;
   }
   ostream_joiner& operator++(int)noexcept {
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

#ifndef utils_hpp
#define utils_hpp

#include <algorithm>

namespace constifier {

template <typename It, typename It2>
It except(It first, It last, It2 rangeFirst, It2 rangeLast) {
   return std::remove_if(first, last, [=](const typename It::value_type& x) {
      return std::find(rangeFirst, rangeLast, x) != rangeLast;
   });
}

template <typename It, typename OutputIt, typename RangeSelector,
          typename Transform>
OutputIt flat_map_transform(It first, It last, OutputIt dest,
                            RangeSelector selector, Transform func) {
   for (; first != last; ++first) {
      auto range = selector(*first);
      for (auto sFirst = std::begin(range); sFirst != std::end(range);
           ++sFirst, ++dest)
         *dest = func(*sFirst);
   }
   return dest;
}

template <typename C, typename P>
void inplace_remove_if(C& ctn, P predicate) {
   ctn.erase(std::remove_if(std::begin(ctn), std::end(ctn), predicate),
             std::end(ctn));
}
}

#endif

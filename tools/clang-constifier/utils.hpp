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

#include <string.h>

struct a_b {
   int a;
   int b;
};

struct foo {
   a_b x;
};


void fct() {
   foo f0;
   foo f1;

   memcpy(&f0.x, &f1.x, sizeof(a_b));
   memcpy(&f0.x, &f1.x, sizeof(f1.x));

   a_b* abs0;
   a_b* abs1;

   memcpy(abs0, abs1, 42 * sizeof(a_b));

   memcpy(abs0, &f1.x, sizeof(a_b));
   int i;
   memcpy(abs0 + i, &f1.x, sizeof(a_b));
}


void pair_like() {
   foo f;
   f.x.a = f.x.b = 0;
}

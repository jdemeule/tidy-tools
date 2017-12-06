
struct foo {
   int x;
};

void fct(int a) {}

void by_value() {
   foo f;

   fct(f.x);
   f.x = 42;
   ++f.x;
   int v = 1 + f.x;
}

void by_pointer() {
   foo* f;

   fct(f->x);
   f->x = 42;
   ++f->x;
   int v = 1 + f->x;
}

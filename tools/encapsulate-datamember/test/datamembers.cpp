
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

void by_ref(foo& f) {
   fct(f.x);
   f.x = 42;
   ++f.x;
   int v = 1 + f.x;
}


struct nested_foo {
   foo f;
};

void take_ptr(int*);
void take_ptr(foo*);
void take_ref(foo&);

void nested() {
   nested_foo nf;
   nf.f.x = 42;

   take_ptr(&nf.f);
   take_ptr(&nf.f.x);
}

void nested_ref() {
   nested_foo nf;
   take_ref(nf.f);
   foo& bis = nf.f;
}

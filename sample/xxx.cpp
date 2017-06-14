// Here the function we want to depreciate.
void foo(int iItemSize,  // <- sizeof(T)
         int iID);

// Here the new function.
template <typename ItemT>
void foo(int iID);


// Let define a type used as 1st parameter of foo.
struct Item {};



void need_replacement() {
   // This call could be replaced automatically.
   foo(sizeof(Item), 42);
}

void need_replacement_but_we_cannot() {
   int item_sz = 21;
   // This call could not be replaced as the 1st parameter is not directly a
   // sizeof expression.
   foo(item_sz, 42);
}

void do_not_need_replacement() {
   // This call is already the right one, no transformation need.
   foo<Item>(42);
}

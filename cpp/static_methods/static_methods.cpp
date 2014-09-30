#include <iostream>
#include <functional>

struct A {
  int identity_const(int i) const {
    std::cout << i << " (const)" << std::endl;
    return i;
  }

  int identity_no_const(int i) {
    std::cout << i << " (no const)" << std::endl;
    return i;
  }
    

  template<typename R>
  static R make_static(std::function<R(A *,int)> fun, A *a, int i) {
    auto func = std::bind(fun, a, i);
    return func();
  }
};

int main() {
  A a;
  a.identity_const(1);

  { std::function<int(A*,int)> f = &A::identity_const;
    auto res = A::make_static<int>(f, &a, 4); }

  { auto res = A::make_static<int>(&A::identity_const, &a, 8); }

  
}

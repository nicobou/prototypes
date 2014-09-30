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
    

  //template<typename R>
  static int make_static(std::function<int(A *,int)> fun, A *a, int i) {
    auto func = std::bind(fun, a, i);
    return func();
  }
};

int main() {
  A a;
  a.identity_const(1);
  //auto truc = std::bind (&A::identity, a, 9);

  std::function<int(A*,int)> f = &A::identity_const;
  int res = A::make_static(f, &a, 4);

  int res2 = A::make_static(&A::identity_const, &a, 8);

  
  // std::cout << res << std::endl; 
}

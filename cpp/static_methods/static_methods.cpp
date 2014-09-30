#include <iostream>
#include <functional>

struct A {
  int identity_const(int i) const {
    std::cout << i << " (const)" << std::endl;
    return i;
  }

  double do_something_const(int a, double b) const {
    double res = a + b + 0.5;
    std::cout << res << std::endl;
    return res;
  }
      
  int identity_no_const(int i) {
    std::cout << i << " (no const)" << std::endl;
    return i;
  }
  
  // template<typename R, typename ...Ts>
  // static R invoke_static(std::function<R(A *,Ts ...)> fun, A *a, Ts ...args) {
  //   auto res = std::bind(fun, a, args...);
  //   return res();
  // }
  
  template<typename R, typename ...Ts>
  static R invoke_static(R(A::*function)(Ts...), A *a, Ts ...args) {
    static_assert(std::is_const<decltype(function)>::value,
                  "invoke_static requires const methods");
    return (*new R);  // for syntax only
  }


  template<typename R, typename ...Ts>
  static R invoke_static(R(A::*function)(Ts...) const, A *a, Ts ...args) {
    std::function<R(A*, Ts...)> fun = function;
    auto res = std::bind(fun, a, args...);
    return res();
  }

};

int main() {
  A a;
  a.identity_const(1);

  // { std::function<int(A*,int)> f = &A::identity_no_const;
  //   auto res = A::invoke_static<int>(f, &a, 4); }
  
  { auto res = A::invoke_static<int>(&A::identity_no_const, &a, 6); }
  
  { auto res = A::invoke_static<int>(&A::identity_const, &a, 6); }
  
  { auto res = A::invoke_static<double>(&A::do_something_const, &a, 6, 7.2); }
  
}

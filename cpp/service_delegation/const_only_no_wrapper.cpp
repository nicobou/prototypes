#include <iostream>
#include <functional>

struct Service {
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
};

class ServiceOwner {
 public:
  // exposing public service const methods
  template<typename R, typename ...Ts>
  R invoke_service(R(Service::*function)(Ts...) const, Ts ...args)
  {
    std::function<R(Service*, Ts...)> fun = function;
    return std::bind(fun, &serv, args...)();
  }

  template<typename R, typename ...Ts>
  R invoke_service(R(Service::*function)(Ts...), Ts ...args)
  {
    // disable invokation of non const members
    static_assert(std::is_const<decltype(function)>::value,
                  "invoke_service requires const methods");
    return (*new R);  // for syntax only since assert should always fail

    // // or the following if non-const methods are allowed 
    // std::function<R(Service*, Ts...)> fun = function;
    // return std::bind(fun, &serv, args...) ();
  }

 private:
  Service serv{};
};

int main() {

  {  // play with service owner
    ServiceOwner owner;
    owner.invoke_service(&Service::identity_const, 6);
    owner.invoke_service(&Service::do_something_const, 5, 8.456);
    // owner.invoke_service(&Service::identity_no_const, 5);
  }
}

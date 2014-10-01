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

template<typename S>
class ConstMethodsInvoker {
public:
  // exposing public service const methods
  template<typename R, typename ...Ts>
  R invoke_service(R(S::*function)(Ts...) const, Ts ...args)
  {
    std::function<R(S *, Ts...)> fun = function;
    return std::bind(fun, get_service_instance(), args...)();
  }

  template<typename R, typename ...Ts>
  R invoke_service(R(S::*function)(Ts...), Ts ...args)
  {
    // disable invokation of non const members
    static_assert(std::is_const<decltype(function)>::value,
                  "invoke_service requires const methods");
    return (*new R);  // for syntax only since assert should always fail
  }

private:
  virtual S *get_service_instance() = 0;
};

class ServiceOwner: public ConstMethodsInvoker<Service> {
  // no wraper
 private:
  Service serv_{};  // RAII member
  Service *get_service_instance() final {return &serv_;}
};

int main() {
    ServiceOwner owner;
    owner.invoke_service(&Service::identity_const, 6);
    owner.invoke_service(&Service::do_something_const, 5, 8.456);
    // will fail with static assert
    // owner.invoke_service(&Service::identity_no_const, 5);
}

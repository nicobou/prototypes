#include <iostream>
#include <functional>
#include <string>

struct Service {
  int identity_const(int i) const {
    std::cout << i << " (const)" << std::endl;
    return i;
  }

  std::string identity_str_const(const std::string &s) const {
    std::cout << s << " (const)" << std::endl;
    return s;
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

  int overloaded_const (int i) const {
    std::cout << i << " (const)" << std::endl;
    return i;
  }

  double overloaded_const (double i) const {
    std::cout << i << " const)" << std::endl;
    return i;
  }


};

template<typename T>
class ConstMethodsInvoker {
public:
  // exposing T const methods accessible by T instance owner
  template<typename R,       // return type
           typename ...DTs,  // Defined arguments types
           typename ...ATs>  // Arguments types
  R cmi_invoke(R(T::*function)(DTs...) const, ATs ...args)
  {
    std::function<R(T *, DTs...)> fun = function;
    return std::bind(std::move(fun),
                     cmi_get(),
                     std::forward<ATs>(args)...)();
  }

  // disable invokation of non const
  template<typename R, typename ...DTs, typename ...ATs>
  R cmi_invoke(R(T::*function)(DTs...), ATs ...args)
  {
    static_assert(std::is_const<decltype(function)>::value,
                  "ConstMethodsInvoker requires const methods only");
    return (*new R);  // for syntax only since assert should always fail
  }

private:
  // require child to pass instance
  virtual T *cmi_get() = 0;
};


class ServiceOwner: public ConstMethodsInvoker<Service> {
  // no wraper
 private:
  Service serv_{};  // RAII member
  Service *cmi_get() final {return &serv_;}
};

int main() {
    ServiceOwner owner;
    owner.cmi_invoke(&Service::identity_const, 6);
    owner.cmi_invoke(&Service::do_something_const, 5, 8.456);
    owner.cmi_invoke(&Service::identity_str_const, "str");
    {
      int (Service::*fun)(int) const = &Service::overloaded_const;
      owner.cmi_invoke(fun, 9);
    }
    {
      double (Service::*fun)(double) const = &Service::overloaded_const;
      owner.cmi_invoke(fun, 10.5);
    }
    owner.cmi_invoke(
        (double(Service::*)(double) const)&Service::overloaded_const,
        11.876);
    // will fail with static assert
    // owner.cmi_invoke(&Service::identity_no_const, 5);
}

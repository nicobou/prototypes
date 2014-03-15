//from http://stackoverflow.com/questions/12877521/human-readable-type-info-name

#include <cxxabi.h>
#include <typeinfo>
#include <iostream>
#include <string>
#include <memory>
#include <cstdlib>

std::string demangle(const char* mangled)
{
      int status;
      std::unique_ptr<char[], void (*)(void*)> result(
        abi::__cxa_demangle(mangled, 0, 0, &status), std::free);
      return result.get() ? std::string(result.get()) : "error occurred";
}

template<class T>
void foo(T t) { std::cout << demangle(typeid(t).name()) << std::endl; }

int main() {
    foo("f");            //char const*
    foo(std::string());  //std::string
}

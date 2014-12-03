#include <string>
#include <iostream>
#include <functional>

#define Delegate_const(_invoc_method, _member_type, _member)            \
  static_assert(std::is_class<_member_type>::value,                     \
                "Delagete_const 2nd arg must be a class");              \
/* exposing T const methods accessible by T instance owner*/            \
  template<typename R,                                                  \
           typename ...ATs>                                             \
  R _invoc_method(R(_member_type::*function)(ATs...) const,             \
                  ATs ...args)                                          \
  {                                                                     \
    std::function<R(_member_type *, ATs...)> fun = function;            \
    return std::bind(std::move(fun),                                    \
                     &_member,                                          \
                     std::forward<ATs>(args)...)();                     \
  }                                                                     \
                                                                        \
  template<typename ...ATs>                                             \
  void                                                                  \
  _invoc_method(void(_member_type::*function)(ATs...) const,            \
                ATs ...args)                                            \
  {                                                                     \
    std::function<void(_member_type *, ATs...)> fun =                   \
        function;                                                       \
    std::bind(std::move(fun),                                           \
              &_member,                                                 \
              std::forward<ATs>(args)...)();                            \
  }                                                                     \
                                                                        \
  /* disable invokation of non const*/                                  \
  template<typename R,                                                  \
           typename ...ATs>                                             \
  R _invoc_method(R(_member_type::*function)(ATs...),                   \
                  ATs ...args)                                          \
  {                                                                     \
    static_assert(std::is_const<decltype(function)>::value,             \
                  "const delegation requires const methods only");      \
    return R();  /* for syntax only since assert should always fail */  \
  }                                                                     \
                                                                        \
  template<typename ...ATs>                                             \
  void _invoc_method(void(_member_type::*function)(ATs...),             \
                     ATs ...args)                                       \
  {                                                                     \
    static_assert(std::is_const<decltype(function)>::value,             \
                  "const delegation requires const methods only");      \
  }                                                                     \

// can be used only after member declaration
#define Delegate_const_no_type(_invoc_method, _member)                  \
  static_assert(std::is_class<decltype(_member)>::value,                \
                "Delegate_const_no_type 2nd arg must"                   \
                "be a declared member");                                \
/* exposing T const methods accessible by T instance owner*/            \
  template<typename R,                                                  \
           typename ...ATs>                                             \
  R _invoc_method(R(decltype(_member)::*function)(ATs...) const,        \
                  ATs ...args)                                          \
  {                                                                     \
    std::function<R(decltype(_member) *, ATs...)> fun = function;       \
    return std::bind(std::move(fun),                                    \
                     &_member,                                          \
                     std::forward<ATs>(args)...)();                     \
  }                                                                     \
                                                                        \
  template<typename ...ATs>                                             \
  void                                                                  \
  _invoc_method(void(decltype(_member)::*function)(ATs...) const,       \
                ATs ...args)                                            \
  {                                                                     \
    std::function<void(decltype(_member) *, ATs...)> fun =              \
        function;                                                       \
    std::bind(std::move(fun),                                           \
              &_member,                                                 \
              std::forward<ATs>(args)...)();                            \
  }                                                                     \
                                                                        \
  /* disable invokation of non const*/                                  \
  template<typename R,                                                  \
           typename ...ATs>                                             \
  R _invoc_method(R(decltype(_member)::*function)(ATs...),              \
                  ATs ...args)                                          \
  {                                                                     \
    static_assert(std::is_const<decltype(function)>::value,             \
                  "const delegation requires const methods only");      \
    return R();  /* for syntax only since assert should always fail */  \
  }                                                                     \
                                                                        \
  template<typename ...ATs>                                             \
  void _invoc_method(void(decltype(_member)::*function)(ATs...),        \
                     ATs ...args)                                       \
  {                                                                     \
    static_assert(std::is_const<decltype(function)>::value,             \
                  "const delegation requires const methods only");      \
  }                                                                     \
  
class Door {
 public:
  Door(std::string str):
      name_(std::move(str)){
  }
  // -------- delegable methods (since const)
  std::string get_name() const {
    return name_;
  }
  std::string get_name(int num) const {
    return name_ + " " + std::to_string(num);
  }
  std::string copy_and_append(int num, std::string str) const {
    return name_ + " " + std::to_string(num) + " " + str;
  }
  void copy_append_and_print(int num, std::string str) const {
    std::printf("%s\n", std::string(name_
                                    + " "
                                    + std::to_string(num)
                                    + " " + str).c_str());
  }
  template<typename T> std::string append_templ(T val) const {
    return name_ + " " + std::to_string(val);
  }
  
  // ---------- not delegable methods (since non-const)
  bool set_name(std::string str) {
    name_ = std::move(str);
    return true;
  }
  void set_name_no_return(std::string str) {
    name_ = std::move(str);
  }

 private:
  std::string name_{"Nico"};
};

class Room {
 public:
  // if Delegate is declared before the member, the member type is required
  Delegate_const(left_door, Door, left_door_);
 private:
  Door right_door_{"right"};
  Door left_door_{"left"};
 public:
  // if declared after, the member type is not required
  Delegate_const_no_type(right_door, right_door_);
};

int main() {
  Room r{};

  { // no need to give args if no overload is creating ambiguity
    std::string res =
        r.right_door<std::string>(&Door::copy_and_append, 3, std::string("coucou"));
    std::printf("%s\n", res.c_str());
  }

  { // using method returning void
    r.right_door<void>(&Door::copy_append_and_print, 4, std::string("hello"));
  }
  
  {  // using right_door_.get_name() 
    std::string res =
        r.right_door<std::string>(&Door::get_name);
    std::printf("%s\n", res.c_str());
  }

  { // using right_door_.get_name(int)
    // arg type (int) is given to the template in order to
    // select the right overload
    std::string res =
        r.right_door<std::string, int>(&Door::get_name, 3);
    std::printf("%s\n", res.c_str());
  }

  { // -do not use this- (verbosely) using right_door_.get_name()
    // but the template arg is the return type only
    std::string res =
        r.right_door<std::string>(
            static_cast<std::string(Door::*)(int) const>(&Door::get_name),
            3);
    std::printf("%s\n", res.c_str());
  }

  { // template member
    // pass arg type in order to get template instanciation
    std::string res =
        r.right_door<std::string, double>(&Door::append_templ<double>, 3.14);
    std::printf("%s\n", res.c_str());
  }
  
  // { // fail to compile since set_name is not const
  //   bool res =
  //       r.right_door<bool>(&Door::set_name, std::string("coucou"));
  //   std::printf("%s\n", std::to_string(res).c_str());
  // }

  // { // fail to compile since set_name is not const
  //   r.right_door<>(&Door::set_name, std::string("coucou"));
  //   std::printf("%s\n", std::to_string(res).c_str());
  // }

  // ------------------------------------------
  // ------------------ the same with left_door
  // ------------------------------------------
  { // no need to give args if no overload is creating ambiguity
    std::string res =
        r.left_door<std::string>(&Door::copy_and_append, 3, std::string("coucou"));
    std::printf("%s\n", res.c_str());
  }

  { // using method returning void
    r.left_door<void>(&Door::copy_append_and_print, 4, std::string("hello"));
  }
  
  {  // using left_door_.get_name() 
    std::string res =
        r.left_door<std::string>(&Door::get_name);
    std::printf("%s\n", res.c_str());
  }

  { // using left_door_.get_name(int)
    // arg type (int) is given to the template in order to
    // select the right overload
    std::string res =
        r.left_door<std::string, int>(&Door::get_name, 3);
    std::printf("%s\n", res.c_str());
  }

  { // -do not use this- (verbosely) using left_door_.get_name()
    // but the template arg is the return type only
    std::string res =
        r.left_door<std::string>(
            static_cast<std::string(Door::*)(int) const>(&Door::get_name),
            3);
    std::printf("%s\n", res.c_str());
  }

  { // template member
    // pass arg type in order to get template instanciation
    std::string res =
        r.left_door<std::string, double>(&Door::append_templ<double>, 3.14);
    std::printf("%s\n", res.c_str());
  }
    
  return 0;
}

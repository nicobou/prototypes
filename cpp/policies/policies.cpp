// template <
//   typename T,
//   template<
//     template<class...> class... Policies
//     >
//   >
// class Base;

#include <string>
#include <deque>
#include <algorithm>
#include <iterator>
#include <iostream>

template<class... Policies>
class PolicyDrivenClass : private Policies...
{
public:
  std::deque<std::string> getNames() const
  {
    return { Policies::name()... };
  } 
};

class Policy1Base
{
public:
  Policy1Base () {std::cout << "policy1base" << std::endl;}
};

template <class T>
class Policy1 : private T
{
public:
  static std::string name(){return "policy 1";}
};

class Policy2Base1
{
public:
  Policy2Base1 () {std::cout << "policy2base1" << std::endl;}
};

class Policy2Base2
{
public:
  Policy2Base2 () {std::cout << "policy2base2" << std::endl;}
};

template <class... Bases>
class Policy2 : private Bases...
{
public:
  static std::string name(){return "policy 2";}
};

class Policy3
{
public:
  static std::string name(){return "policy 3";}
};

///////////////////////////////////////
class Base
{
public :
  virtual std::string do_something () = 0;
};

template<class... Policies>
class ConfigurableBase : public Base, public PolicyDrivenClass <Policies...>
{};

class Leaf : public ConfigurableBase<
  Policy1<Policy1Base>, 
  Policy2<Policy2Base1, Policy2Base2>, 
  Policy3
  >
{
public :
  std::string do_something () {return "do_something Leaf";}
};

///////////////////////////////////////

int main()
{
  PolicyDrivenClass
    <
      Policy1<Policy1Base>, 
      Policy2<Policy2Base1, Policy2Base2>, 
      Policy3
      > 
  c;
std::deque<std::string> names = c.getNames();

std::ostream_iterator<std::string> out (std::cerr,"\n");
std::copy(names.begin(), names.end(), out);

Leaf leaf;
std::cout << leaf.do_something () << std::endl;
std::deque<std::string> leafnames = leaf.getNames();
std::copy(leafnames.begin(), leafnames.end(), out);
}

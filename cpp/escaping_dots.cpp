#include <string>
#include <algorithm>
#include <iostream>

std::string escape_dots(const std::string &str) {
  // replacing dots in name by __DOT__
  std::string escaped = std::string();
  std::size_t i = 0;
  while (std::string::npos != i) {
    auto found = str.find('.', i);
    if (i != found)
      escaped += std::string(str, i, found - i);
    if (std::string::npos != found) {
      escaped += "__DOT__";
      i = ++found;
    } else {
      i = std::string::npos;
    }
  }
  return escaped;
}

std::string unescape_dots(const std::string &str) {
  std::string unescaped = std::string();
  std::string esc_char("__DOT__");
  std::size_t i = 0;
  while(std::string::npos != i) {
    std::size_t found = str.find(esc_char, i);
    if (i != found)
      unescaped += std::string(str, i , found - i);
    if (std::string::npos != found) {
      unescaped += ".";
      i = found + esc_char.size();
    } else {
      i = std::string::npos;
    }
  }
  return unescaped;
}

int main() {

  {
    std::string str("..an..example..");
    std::printf("%s\n", str.c_str());
    std::printf("%s\n", escape_dots(str).c_str());
    std::printf("unescaping %s\n", unescape_dots("an example").c_str());
    std::printf("%s\n", unescape_dots(escape_dots(str)).c_str());
  }
  std::printf("-----------\n");
  {
    std::string str("me@example.net");
    std::printf("%s\n", str.c_str());
    std::printf("%s\n", escape_dots(str).c_str());
    std::printf("unescaping %s\n", unescape_dots("an example").c_str());
    std::printf("%s\n", unescape_dots(escape_dots(str)).c_str());
  }
  return 0;
}

#pragma once
#include <type_traits>
#include <string>

namespace msg
{

template <typename T, typename = void>
struct has_str : std::false_type {};

template <typename T>
struct has_str<T, std::void_t<decltype(std::declval<const T>().str())>>
  : std::is_same<decltype(std::declval<const T>().str()), std::string> {};

template <typename T>
constexpr bool has_str_v = has_str<T>::value;

} // namespace msg

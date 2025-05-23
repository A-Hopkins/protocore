#pragma once
#include <type_traits>
#include <string>

namespace msg
{

template <typename T, typename = void>
struct has_to_string : std::false_type {};

template <typename T>
struct has_to_string<T, std::void_t<decltype(std::declval<const T>().to_string())>>
  : std::is_same<decltype(std::declval<const T>().to_string()), std::string> {};

template <typename T>
constexpr bool has_to_string_v = has_to_string<T>::value;

} // namespace msg

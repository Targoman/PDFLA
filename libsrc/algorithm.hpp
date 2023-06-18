#ifndef __TARGOMAN_COMMON_ALGORITHMS__
#define __TARGOMAN_COMMON_ALGORITHMS__

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <numeric>
#include <vector>

#include "type_traits.hpp"

namespace Targoman {
namespace Common {

template <typename T0, typename Functor_t>
auto map(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  using T1 = decltype(std::declval<Functor_t>()(
      std::declval<typename ContainerTraits_t::ElementType_t>()));
  std::vector<T1> Result;
  std::transform(v.begin(), v.end(), std::back_inserter(Result), f);
  return Result;
}

template <typename T0, typename Functor_t>
auto filter(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`filter` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  std::vector<T1> Result;
  std::copy_if(v.begin(), v.end(), std::back_inserter(Result), f);
  return Result;
}

template <typename T0, typename Functor_t>
void filter_(T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`filter` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  v.erase(
      std::remove_if(v.begin(), v.end(), [&f](const T1 &e) { return !f(e); }),
      v.end());
}

template <typename T0>
void filterNullPtrs_(T0 &v) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "filterNullPtrs_ only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  v.erase(std::remove_if(v.begin(), v.end(),
                         [](const T1 &e) { return e == nullptr; }),
          v.end());
}

template <typename T0, typename Functor_t>
auto split(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`split` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  std::vector<T1> Conforms, DoesNotConform;
  std::partition_copy(v.begin(), v.end(), std::back_inserter(Conforms),
                      std::back_inserter(DoesNotConform), f);
  return std::make_tuple(Conforms, DoesNotConform);
}

template <typename T0, typename Functor_t>
const auto &maxElement(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`maxElement` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  return *std::max_element(v.begin(), v.end(), [&f](const T1 &a, const T1 &b) {
    return f(a) < f(b);
  });
}

template <typename T0, typename Functor_t>
auto argmax(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`argmax` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  return std::distance(
      v.begin(),
      std::max_element(v.begin(), v.end(),
                       [&f](const T1 &a, const T1 &b) { return f(a) < f(b); }));
}

template <typename T0, typename Functor_t>
const auto &minElement(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`minElement` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  return *std::min_element(v.begin(), v.end(), [&f](const T1 &a, const T1 &b) {
    return f(a) < f(b);
  });
}

template <typename T0, typename Functor_t>
auto argmin(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`argmin` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  return std::distance(
      v.begin(),
      std::min_element(v.begin(), v.end(),
                       [&f](const T1 &a, const T1 &b) { return f(a) < f(b); }));
}

template <typename T0, typename Functor_t>
auto max(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`max` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  using T2 =
      BareType_t<decltype(std::declval<Functor_t>()(std::declval<T1>()))>;
  if (v.empty()) return T2();
  return f(*std::max_element(
      v.begin(), v.end(),
      [&f](const T1 &a, const T1 &b) { return f(a) < f(b); }));
}

template <typename T0, typename Functor_t>
auto min(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`min` only works on containers");
  using T1 = typename ContainerTraits_t::ElementBareType_t;
  using T2 =
      BareType_t<decltype(std::declval<Functor_t>()(std::declval<T1>()))>;
  if (v.empty()) return T2();
  return f(*std::min_element(
      v.begin(), v.end(),
      [&f](const T1 &a, const T1 &b) { return f(a) < f(b); }));
}

template <typename T0, typename Functor_t>
auto mean(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`mean` only works on containers");
  using T1 = BareType_t<decltype(std::declval<Functor_t>()(
      std::declval<typename ContainerTraits_t::ElementType_t>()))>;
  if (v.empty()) return static_cast<T1>(0);
  return std::accumulate(v.begin(), v.end(), static_cast<T1>(0),
                         [&f](auto sum, auto e) { return sum + f(e); }) /
         static_cast<T1>(v.size());
}

template <typename T>
auto mean(const T &v) {
  return mean(v, [](auto e) { return e; });
}

template <typename T0, typename Functor_t>
auto meanAndVar(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`var` only works on containers");
  using T1 = BareType_t<decltype(std::declval<Functor_t>()(
      std::declval<typename ContainerTraits_t::ElementType_t>()))>;
  if (v.empty()) return std::make_tuple<T1, T1>(0, 0);
  T1 Mean = mean(v, f);
  return std::make_tuple<T1, T1>(
      static_cast<T1>(Mean),
      std::accumulate(
          v.begin(), v.end(), static_cast<T1>(0),
          [&f, &Mean](auto sum, auto e) { return sum + (f(e) - Mean); }) /
          static_cast<T1>(v.size()));
}

template <typename T0, typename Functor_t>
auto var(const T0 &v, Functor_t f) {
  auto [Mean, Var] = meanAndVar(v, f);
  return Var;
}

template <typename T0, typename Functor_t>
auto meanAndStd(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`std` only works on containers");
  using T1 = BareType_t<decltype(std::declval<Functor_t>()(
      std::declval<typename ContainerTraits_t::ElementType_t>()))>;
  auto [Mean, Var] = meanAndVar(v, f);
  if (Var > std::numeric_limits<T1>::epsilon())
    return std::make_pair<T1, T1>(static_cast<T1>(Mean), sqrt(Var));
  return std::make_pair<T1, T1>(static_cast<T1>(Mean), 0);
}

template <typename T>
auto meanAndStd(const T &v) {
  return meanAndStd(v, [](auto e) { return e; });
}

template <typename T0, typename Functor_t>
auto std(const T0 &v, Functor_t f) {
  auto [Mean, Std] = meanAndStd(v, f);
  return Std;
}

template <typename T0, typename T1>
auto cat(const T0 &a, const T1 &b) {
  using ContainerTraits0_t = stuContainerTypeTraits<T0>;
  using ContainerTraits1_t = stuContainerTypeTraits<T1>;
  static_assert(
      ContainerTraits0_t::IsContainer && ContainerTraits1_t::IsContainer,
      "`cat` only works on containers");
  using T2 = BareType_t<decltype(*std::declval<T0>().begin())>;
  static_assert(
      std::is_same_v<T2, BareType_t<decltype(*std::declval<T1>().begin())>>,
      "Both containers must have same element type for `cat` to work");
  std::vector<T2> Result;
  Result.insert(Result.end(), a.begin(), a.end());
  Result.insert(Result.end(), b.begin(), b.end());
  return Result;
}

template <typename T0, typename Functor_t>
bool any(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`any` only works on containers");
  if (v.empty()) return false;
  return std::any_of(v.begin(), v.end(), f);
}

template <typename T0, typename Functor_t>
bool all(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`all` only works on containers");
  if (v.empty()) return false;
  return std::all_of(v.begin(), v.end(), f);
}

template <typename T0, typename Functor_t>
bool none(const T0 &v, Functor_t f) {
  using ContainerTraits_t = stuContainerTypeTraits<T0>;
  static_assert(ContainerTraits_t::IsContainer,
                "`none` only works on containers");
  if (v.empty()) return false;
  return std::none_of(v.begin(), v.end(), f);
}

}  // namespace Common
}  // namespace Targoman

#endif  //__TARGOMAN_COMMON_ALGORITHMS__
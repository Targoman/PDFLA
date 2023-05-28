#ifndef __TARGOMAN_COMMON_ALGORITHMS__
#define __TARGOMAN_COMMON_ALGORITHMS__

#include <stdint.h>

#include <limits>
#include <vector>

namespace Targoman {
namespace Common {

template <typename T0, typename Functor_t>
auto map(const std::vector<T0> &v, Functor_t f) {
  using T1 = decltype(std::declval<Functor_t>()(std::declval<T0>()));
  std::vector<T1> Result;
  Result.reserve(v.size());
  for (const auto &Item : v) Result.push_back(f(Item));
  return Result;
}

template <typename T, typename Functor_t>
std::vector<T> filter(const std::vector<T> &v, Functor_t f) {
  std::vector<T> Result;
  Result.reserve(v.size());
  for (const auto &Item : v)
    if (f(Item)) Result.push_back(Item);
  Result.shrink_to_fit();
  return Result;
}

template <typename T, typename Functor_t>
std::tuple<std::vector<T>, std::vector<T>> split(const std::vector<T> &v,
                                                 Functor_t f) {
  std::vector<T> Conforms, DoesNotConform;
  for (const auto &Item : v)
    if (f(Item))
      Conforms.push_back(Item);
    else
      DoesNotConform.push_back(Item);
  return std::make_tuple(Conforms, DoesNotConform);
}

template <typename T, typename Functor_t>
const T &max(const std::vector<T> &v, Functor_t f) {
  const T *Max = &v[0];
  auto Value = f(v[0]);
  for (size_t i = 1; i < v.size(); ++i) {
    auto NewValue = f(v[i]);
    if (NewValue > Value) {
      Max = &v[i];
      Value = NewValue;
    }
  }
  return *Max;
}

template <typename T, typename Functor_t>
int32_t argmax(const std::vector<T> &v, Functor_t f) {
  int32_t Result = 0;
  auto Value = f(v[0]);
  for (size_t i = 1; i < v.size(); ++i) {
    auto NewValue = f(v[i]);
    if (NewValue > Value) {
      Result = i;
      Value = NewValue;
    }
  }
  return Result;
}

template <typename T, typename Functor_t>
const T &min(const std::vector<T> &v, Functor_t f) {
  const T *Min = &v[0];
  auto Value = f(v[0]);
  for (size_t i = 1; i < v.size(); ++i) {
    auto NewValue = f(v[i]);
    if (NewValue < Value) {
      Min = &v[i];
      Value = NewValue;
    }
  }
  return *Min;
}

template <typename T, typename Functor_t>
int32_t argmin(const std::vector<T> &v, Functor_t f) {
  int32_t Result = 0;
  auto Value = f(v[0]);
  for (size_t i = 1; i < v.size(); ++i) {
    auto NewValue = f(v[i]);
    if (NewValue < Value) {
      Result = i;
      Value = NewValue;
    }
  }
  return Result;
}

template <typename T0, typename Functor_t>
auto mean(const std::vector<T0> &v, Functor_t f) {
  using T1 = decltype(std::declval<Functor_t>()(std::declval<T0>()));
  T1 Result = 0;
  for (const auto &e : v) Result += f(e);
  if (v.size() > 0) Result /= static_cast<T1>(v.size());
  return Result;
}

template <typename T0, typename Functor_t>
auto var(const std::vector<T0> &v, Functor_t f) {
  using T1 = decltype(std::declval<Functor_t>()(std::declval<T0>()));
  T1 Mean = 0;
  T1 MeanSquares = 0;
  for (const auto &e : v) {
    Mean += f(e);
    MeanSquares += f(e) * f(e);
  }
  if (v.size() == 0) return static_cast<T1>(0);
  T1 N = static_cast<T1>(v.size());
  MeanSquares /= N;
  Mean /= N;
  return (MeanSquares - Mean * Mean);
}

template <typename T0, typename Functor_t>
auto std(const std::vector<T0> &v, Functor_t f) {
  using T1 = decltype(std::declval<Functor_t>()(std::declval<T0>()));
  auto Var = var(v, f);
  if (Var > std::numeric_limits<T1>::epsilon()) return sqrt(Var);
  return 0;
}

template <typename T>
std::vector<T> cat(const std::vector<T> &a, const std::vector<T> &b) {
  std::vector<T> Result = a;
  Result.insert(Result.end(), b.begin(), b.end());
  return Result;
}

}  // namespace Common
}  // namespace Targoman

#endif  //__TARGOMAN_COMMON_ALGORITHMS__
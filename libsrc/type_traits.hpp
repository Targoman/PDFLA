#ifndef __TARGOMAN_COMMON_TYPE_TRAITS__
#define __TARGOMAN_COMMON_TYPE_TRAITS__

#include <memory>
#include <type_traits>

namespace Targoman {
namespace Common {

template <typename T>
using BareType_t =
    std::remove_pointer_t<std::remove_reference_t<std::remove_cv_t<T>>>;

template <typename T>
struct stuSharedPtrTypeTraits {
  static constexpr bool IsSharedPtr = false;
  typedef T ElementType_t;
  typedef BareType_t<T> ElementBareType_t;
};

template <typename T>
struct stuSharedPtrTypeTraits<std::shared_ptr<T>> {
  static constexpr bool IsSharedPtr = true;
  typedef T ElementType_t;
  typedef BareType_t<T> ElementBareType_t;
};

template<typename T>
constexpr bool IsSharedPtr = stuSharedPtrTypeTraits<T>::IsSharedPtr; 

template <typename T, typename=void>
struct stuContainerTypeTraits {
  typedef T ElementType_t;
  typedef BareType_t<ElementType_t> ElementBareType_t;
  static constexpr bool IsContainer = false;
};

template <typename T>
struct stuContainerTypeTraits<T, decltype((void)std::declval<T>().begin())> {
  using ElementType_t = decltype(*std::declval<T>().begin());
  typedef BareType_t<ElementType_t> ElementBareType_t;
  static constexpr bool IsContainer = true;
  static constexpr bool ContainsSharedPtr =
      stuSharedPtrTypeTraits<ElementBareType_t>::IsSharedPtr;
  static constexpr bool ContainsPointerOrSharedPtr =
      ContainsSharedPtr || std::is_pointer_v<ElementType_t>;
  using stuOnlyForPointers = std::enable_if<ContainsPointerOrSharedPtr == true>;

  template <typename RequiredBase_t>
  using stuOnlyForNonPointers =
      std::enable_if<ContainsPointerOrSharedPtr == false &&
                       std::is_base_of_v<RequiredBase_t, ElementBareType_t>>;
};

template<typename T, typename=void>
constexpr bool IsContainer = stuContainerTypeTraits<T>::IsContainer;

}  // namespace Common
}  // namespace Targoman

#endif  // __TARGOMAN_COMMON_TYPE_TRAITS__
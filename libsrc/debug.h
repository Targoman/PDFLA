#ifndef __TARGOMAN_PDFLA_DEBUG__
#define __TARGOMAN_PDFLA_DEBUG__

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "pdfla.h"
#include "type_traits.hpp"

namespace Targoman {
namespace PDFLA {

constexpr float DEBUG_UPSCALE_FACTOR = 4.f;

typedef std::tuple<std::vector<uint8_t>, Targoman::DLA::stuSize> RawImageData_t;

template <typename T>
using RemoveConstRef_t = std::remove_const_t<std::remove_reference_t<T>>;

template <typename T, typename = void>
struct stuBoundingBoxPtrHelper {};

template <typename T>
struct stuBoundingBoxPtrHelper<T*, void> {
  static void appendTo(const T*& _item,
                       std::vector<const Targoman::DLA::stuBoundingBox*>& v) {
    if (_item != nullptr)
      stuBoundingBoxPtrHelper<RemoveConstRef_t<T>>::appendTo(*_item, v);
  }
};

template <typename T>
struct stuBoundingBoxPtrHelper<std::shared_ptr<T>, void> {
  static void appendTo(const std::shared_ptr<T>& _item,
                       std::vector<const Targoman::DLA::stuBoundingBox*>& v) {
    if (_item.get() != nullptr)
      stuBoundingBoxPtrHelper<RemoveConstRef_t<T>>::appendTo(*_item, v);
  }
};

template <typename T>
struct stuBoundingBoxPtrHelper<std::reference_wrapper<T>, void> {
  static void appendTo(const std::reference_wrapper<T>& _item,
                       std::vector<const Targoman::DLA::stuBoundingBox*>& v) {
      stuBoundingBoxPtrHelper<RemoveConstRef_t<T>>::appendTo(_item.get(), v);
  }
};

template <typename T>
struct stuBoundingBoxPtrHelper<
    T,
    std::enable_if_t<
        std::is_same_v<std::shared_ptr<typename T::element_type>, T> == false &&
        std::is_base_of_v<std::shared_ptr<typename T::element_type>, T>>> {
  static void appendTo(const T& _item,
                       std::vector<const Targoman::DLA::stuBoundingBox*>& v) {
    if (_item.get() != nullptr)
      stuBoundingBoxPtrHelper<typename T::element_type>::appendTo(*_item, v);
  }
};

template <typename T, typename = void>
struct stuHasBoundingBoxMember : public std::false_type {};

template <typename T>
struct stuHasBoundingBoxMember<T, decltype((void)&T::BoundingBox)>
    : public std::true_type {};

template <typename T>
constexpr bool HasBoundingBoxMember = stuHasBoundingBoxMember<T>::value;

template <>
struct stuBoundingBoxPtrHelper<Targoman::DLA::stuBoundingBox, void> {
  static void appendTo(const Targoman::DLA::stuBoundingBox& _item,
                       std::vector<const Targoman::DLA::stuBoundingBox*>& v) {
    v.push_back(&_item);
  }
};

template <typename T>
struct stuBoundingBoxPtrHelper<T, std::enable_if_t<HasBoundingBoxMember<T>>> {
  static void appendTo(const T& _item,
                       std::vector<const Targoman::DLA::stuBoundingBox*>& v) {
    stuBoundingBoxPtrHelper<Targoman::DLA::stuBoundingBox>::appendTo(
        _item.BoundingBox, v);
  }
};

template <typename T>
struct stuBoundingBoxPtrHelper<
    T, std::enable_if_t<Targoman::Common::IsContainer<T>>> {
  static void appendTo(const T& _container,
                       std::vector<const Targoman::DLA::stuBoundingBox*>& v) {
    using U = RemoveConstRef_t<decltype(*_container.begin())>;
    for (const auto& Item : _container)
      stuBoundingBoxPtrHelper<U>::appendTo(Item, v);
  }
};

struct stuPdfLaDebugImageData;
class clsPdfLaDebugImage {
 private:
  std::shared_ptr<stuPdfLaDebugImageData> Data;

 public:
  clsPdfLaDebugImage(const RawImageData_t& _imageData,
                     const std::string& _debugOutputPath,
                     const std::string& _basename, size_t _pageIndex);

  clsPdfLaDebugImage();

  template <typename... Ts>
  clsPdfLaDebugImage& add(const Ts&... _items) {
    std::vector<const Targoman::DLA::stuBoundingBox*> BoundingBoxes;
    auto finalize = [this, &BoundingBoxes]() mutable {
      if (BoundingBoxes.size() == 0) return;
      this->add(BoundingBoxes);
      BoundingBoxes.clear();
    };
    auto consume = [this, &BoundingBoxes, &finalize](auto& _item) mutable {
      using U = RemoveConstRef_t<decltype(_item)>;
      if constexpr (Targoman::Common::IsContainer<U>) {
        finalize();
        stuBoundingBoxPtrHelper<U>::appendTo(_item, BoundingBoxes);
        finalize();
      } else {
        stuBoundingBoxPtrHelper<U>::appendTo(_item, BoundingBoxes);
      }
    };
    (..., (consume(_items)));
    finalize();
    return *this;
  }

  template <typename... Ts>
  clsPdfLaDebugImage& add(const std::initializer_list<Ts>&... _items) {
    std::vector<const Targoman::DLA::stuBoundingBox*> BoundingBoxes;
    auto finalize = [this, &BoundingBoxes]() mutable {
      if (BoundingBoxes.size() == 0) return;
      this->add(BoundingBoxes);
      BoundingBoxes.clear();
    };
    auto consume = [this, &BoundingBoxes,
                    &finalize](const auto& _items) mutable {
      using U = RemoveConstRef_t<decltype(*_items.begin())>;
      if constexpr (Targoman::Common::IsContainer<U>)
        for (const auto& Item : _items) {
          finalize();
          stuBoundingBoxPtrHelper<U>::appendTo(Item, BoundingBoxes);
          finalize();
        }
      else {
        finalize();
        for (const auto& Item : _items) {
          stuBoundingBoxPtrHelper<U>::appendTo(Item, BoundingBoxes);
        }
        finalize();
      }
    };
    (..., (consume(_items)));
    return *this;
  }

  clsPdfLaDebugImage& add(
      const std::vector<const Targoman::DLA::stuBoundingBox*>& _boundingBoxes);

  clsPdfLaDebugImage& show(const std::string& _tag);
  clsPdfLaDebugImage& save(const std::string& _tag);
};

struct stuPdfLaDebugData;
class clsPdfLaDebug {
 private:
  std::string DebugOutputPath;
  std::map<const void*, stuPdfLaDebugData> DebugData;

 private:
  clsPdfLaDebug();

 public:
  static clsPdfLaDebug& instance();

 public:
  template <typename T>
  void registerObject(T* _object, const std::string& _basename) {
    this->registerObject(static_cast<const void*>(_object), _basename);
  }

  template <typename T>
  void unregisterObject(T* _object) {
    this->unregisterObject(static_cast<const void*>(_object));
  }

  template <typename T>
  bool isObjectRegistered(T* _object) {
    return this->isObjectRegistered<const void>(
        static_cast<const void*>(_object));
  }

  template <typename T>
  bool pageImageExists(T* _object, size_t _pageIndex) {
    return this->pageImageExists<const void>(static_cast<const void*>(_object),
                                             _pageIndex);
  }

  template <typename T>
  void registerPageImage(T* _object, size_t _pageIndex,
                         const std::vector<uint8_t>& _data,
                         const Targoman::DLA::stuSize& _size) {
    this->registerPageImage(static_cast<const void*>(_object), _pageIndex,
                            _data, _size);
  }

  template <typename T>
  const RawImageData_t& getPageImage(T* _object, size_t _pageIndex) {
    return this->getPageImage(static_cast<const void*>(_object), _pageIndex);
  }

  template <typename T>
  void setCurrentPageIndex(T* _object, size_t _pageIndex) {
    this->setCurrentPageIndex(static_cast<const void*>(_object), _pageIndex);
  }

  template <typename T>
  clsPdfLaDebugImage createImage(T* _object) {
    return this->createImage(static_cast<const void*>(_object));
  }

 public:
  void setDebugOutputPath(const std::string& _debugOutputPath);
};

template <>
clsPdfLaDebugImage clsPdfLaDebug::createImage(const void* _object);

template <>
void clsPdfLaDebug::registerObject(const void* _object,
                                   const std::string& _basename);

template <>
void clsPdfLaDebug::unregisterObject(const void* _object);

template <>
bool clsPdfLaDebug::isObjectRegistered(const void* _object);

template <>
bool clsPdfLaDebug::pageImageExists(const void* _object, size_t _pageIndex);

template <>
void clsPdfLaDebug::registerPageImage(const void* _object, size_t _pageIndex,
                                      const std::vector<uint8_t>& _data,
                                      const Targoman::DLA::stuSize& _size);

template <>
const RawImageData_t& clsPdfLaDebug::getPageImage(const void* _object,
                                                  size_t _pageIndex);

template <>
void clsPdfLaDebug::setCurrentPageIndex(const void* _object, size_t _pageIndex);

}  // namespace PDFLA
}  // namespace Targoman

#endif  // __TARGOMAN_PDFLA_DEBUG__
#ifndef __TARGOMAN_PDFLA_DEBUG__
#define __TARGOMAN_PDFLA_DEBUG__

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "pdfla.h"

namespace Targoman {
namespace PDFLA {

typedef std::tuple<std::vector<uint8_t>, Targoman::DLA::stuSize> RawImageData_t;

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
  bool isObjectRegister(T* _object) {
    return this->isObjectRegister(static_cast<const void*>(_object));
  }

  template <typename T>
  bool pageImageExists(T* _object, size_t _pageIndex) {
    return this->pageImageExists(static_cast<const void*>(_object), _pageIndex);
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

  void saveDebugImage(
      const void* _object, const std::string& _tag,
      const std::vector<std::vector<const Targoman::DLA::stuBoundingBox*>>&
          _boundingBoxes,
      float _scale);

  void showDebugImage(
      const void* _object, const std::string& _tag,
      const std::vector<std::vector<const Targoman::DLA::stuBoundingBox*>>&
          _boundingBoxes,
      float _scale);

  template <typename T>
  std::vector<const Targoman::DLA::stuBoundingBox*>
  getVectorOfPointersToBoundingBoxes(const T& _container) {
    std::vector<const Targoman::DLA::stuBoundingBox*> Result;
    for (const auto& Item : _container) Result.push_back(&(Item->BoundingBox));
    return Result;
  }

  std::vector<const Targoman::DLA::stuBoundingBox*>
  getVectorOfPointersToBoundingBoxes(
      const Targoman::DLA::BoundingBoxPtrVector_t& _container) {
    std::vector<const Targoman::DLA::stuBoundingBox*> Result;
    for (const auto& Item : _container) Result.push_back(Item.get());
    return Result;
  }

  template <typename T, typename... Ts>
  void saveDebugImage(T* _object, const std::string& _tag, float _scale,
                      const Ts&... _boundingBoxes) {
    std::vector<std::vector<const Targoman::DLA::stuBoundingBox*>>
        BoundingBoxes;
    (..., (BoundingBoxes.push_back(
              this->getVectorOfPointersToBoundingBoxes(_boundingBoxes))));
    this->saveDebugImage(static_cast<const void*>(_object), _tag, BoundingBoxes,
                         _scale);
  }

  template <typename T, typename... Ts>
  void showDebugImage(T* _object, const std::string& _tag, float _scale,
                      const Ts&... _boundingBoxes) {
    std::vector<std::vector<const Targoman::DLA::stuBoundingBox*>>
        BoundingBoxes;
    (..., (BoundingBoxes.push_back(
              this->getVectorOfPointersToBoundingBoxes(_boundingBoxes))));
    this->showDebugImage(static_cast<const void*>(_object), _tag, BoundingBoxes,
                         _scale);
  }

 public:
  void setDebugOutputPath(const std::string& _debugOutputPath);

  friend class clsPdfLaDebugImageHelper;
};

}  // namespace PDFLA
}  // namespace Targoman

#endif  // __TARGOMAN_PDFLA_DEBUG__
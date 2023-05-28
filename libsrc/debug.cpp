#include "debug.h"

#include <opencv4/opencv2/opencv.hpp>
#include <sstream>

namespace Targoman {
namespace PDFLA {
using namespace Targoman::DLA;

struct stuPdfLaDebugData {
  std::map<size_t, RawImageData_t> PageImages;
  std::string ObjectBasename;
  size_t CurrentPageIndex;
};

clsPdfLaDebug::clsPdfLaDebug() {}

clsPdfLaDebug& clsPdfLaDebug::instance() {
  static std::unique_ptr<clsPdfLaDebug> Instance;
  if (__glibc_unlikely(Instance.get() == nullptr))
    Instance.reset(new clsPdfLaDebug);
  return *Instance;
}

template <>
void clsPdfLaDebug::registerObject(const void* _object,
                                   const std::string& _basename) {
  if (this->DebugData.find(_object) == this->DebugData.end())
    this->DebugData[_object] = stuPdfLaDebugData{
        std::map<size_t, std::tuple<std::vector<uint8_t>, stuSize>>(),
        _basename, 0};
}

template <>
void clsPdfLaDebug::unregisterObject(const void* _object) {
  if (this->DebugData.find(_object) != this->DebugData.end())
    this->DebugData.erase(_object);
}

template <>
bool clsPdfLaDebug::isObjectRegister(const void* _object) {
  return this->DebugData.find(_object) != this->DebugData.end();
}

template <>
bool clsPdfLaDebug::pageImageExists(const void* _object, size_t _pageIndex) {
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return false;
  auto& PageImages = DebugDataIterator->second.PageImages;
  return PageImages.find(_pageIndex) != PageImages.end();
}

cv::Mat bufferToMat(const std::vector<uint8_t>& _data, const stuSize& _size) {
  cv::Mat Result(_size.Height, _size.Width, CV_8UC3);
  size_t Height = static_cast<size_t>(_size.Height);
  size_t Width = static_cast<size_t>(_size.Width);
  auto SourceData = _data.data();
  auto MatrixData = Result.data;
  for (size_t Row = 0; Row < Height; ++Row) {
    memcpy(MatrixData, SourceData, Width * 3);
    MatrixData += Result.step;
    SourceData += Width * 3;
  }
  return Result;
}

template <>
void clsPdfLaDebug::registerPageImage(const void* _object, size_t _pageIndex,
                                      const std::vector<uint8_t>& _data,
                                      const stuSize& _size) {
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return;
  DebugDataIterator->second.PageImages[_pageIndex] =
      std::make_tuple(_data, _size);
}

template <>
const RawImageData_t& clsPdfLaDebug::getPageImage(const void* _object,
                                                  size_t _pageIndex) {
  static RawImageData_t Empty;
  using T = typename decltype(this->DebugData)::mapped_type;
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return Empty;
  auto& PageImages = DebugDataIterator->second.PageImages;
  auto PageImageIterator = PageImages.find(_pageIndex);
  if (PageImageIterator == PageImages.end()) return Empty;
  return PageImageIterator->second;
}

template <>
void clsPdfLaDebug::setCurrentPageIndex(const void* _object,
                                        size_t _pageIndex) {
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return;
  DebugDataIterator->second.CurrentPageIndex = _pageIndex;
}

cv::Rect bbox2CvRect(const stuBoundingBox& _bbox) {
  return cv::Rect(static_cast<int>(_bbox.left()), static_cast<int>(_bbox.top()),
                  static_cast<int>(_bbox.width()),
                  static_cast<int>(_bbox.height()));
}

class clsPdfLaDebugImageHelper {
 public:
  static auto createDebugImage(
      clsPdfLaDebug* _pdfLaDebug, const void* _object, const std::string& _tag,
      const std::vector<std::vector<const stuBoundingBox*>>& _boundingBoxes,
      float _scale) {
    using T = std::tuple<cv::Mat, std::string, size_t>;
    constexpr int COLORS[][3] = {
        {0, 80, 0}, {0, 0, 255}, {80, 20, 150}, {0, 180, 180}};
    constexpr size_t COLORS_SIZE = sizeof COLORS / sizeof COLORS[0];

    auto DebugDataIterator = _pdfLaDebug->DebugData.find(_object);
    if (DebugDataIterator == _pdfLaDebug->DebugData.end()) return T();

    auto& PageImages = DebugDataIterator->second.PageImages;
    auto PageImagesIterator =
        PageImages.find(DebugDataIterator->second.CurrentPageIndex);
    if (PageImagesIterator == PageImages.end()) return T();

    auto [Data, Size] = PageImagesIterator->second;
    cv::Mat PageImage = bufferToMat(Data, Size);
    size_t ColorIndex = 0;
    for (auto& BoundingBoxVector : _boundingBoxes) {
      cv::Scalar FillColor(COLORS[ColorIndex][0], COLORS[ColorIndex][1],
                           COLORS[ColorIndex][2]);
      for (auto& BoundingBox : BoundingBoxVector) {
        cv::Rect Rect = bbox2CvRect(BoundingBox->scale(_scale));
        if (Rect.area() < 4) continue;
        cv::Mat ROI(PageImage, Rect);
        cv::Mat RoiCopy = ROI.clone();
        cv::rectangle(RoiCopy, cv::Rect(0, 0, Rect.width, Rect.height),
                      FillColor, -1);
        cv::addWeighted(ROI, 0.3, RoiCopy, 0.7, 0, ROI);
      }
      ColorIndex = (++ColorIndex) % COLORS_SIZE;
    }
    return std::make_tuple(PageImage, DebugDataIterator->second.ObjectBasename,
                           DebugDataIterator->second.CurrentPageIndex);
  }
};

void clsPdfLaDebug::saveDebugImage(
    const void* _object, const std::string& _tag,
    const std::vector<std::vector<const stuBoundingBox*>>& _boundingBoxes,
    float _scale) {
  auto [PageImage, Basename, CurrentPageIndex] =
      clsPdfLaDebugImageHelper::createDebugImage(this, _object, _tag,
                                                 _boundingBoxes, _scale);
  if (PageImage.empty()) return;
  std::ostringstream ss;
  ss << this->DebugOutputPath << "/" << Basename << "_" << _tag << "_p"
     << std::setw(3) << CurrentPageIndex << ".png";

  cv::imwrite(ss.str(), PageImage);
}

void clsPdfLaDebug::showDebugImage(
    const void* _object, const std::string& _tag,
    const std::vector<std::vector<const stuBoundingBox*>>& _boundingBoxes,
    float _scale) {
  auto [PageImage, Basename, CurrentPageIndex] =
      clsPdfLaDebugImageHelper::createDebugImage(this, _object, _tag,
                                                 _boundingBoxes, _scale);
  if (PageImage.empty()) return;
  std::ostringstream ss;
  ss << Basename << "_" << _tag << "_p" << std::setw(3) << CurrentPageIndex
     << ".png";

  float X0 = NAN, Y0 = NAN, X1 = NAN, Y1 = NAN;
  for(auto& v : _boundingBoxes)
    for(auto& e : v) {
      if(isnan(X0) || e->left() < X0)
        X0 = e->left();
      if(isnan(Y0) || e->top() < X0)
        Y0 = e->top();
      if(isnan(X1) || e->right() > X1)
        X1 = e->right();
      if(isnan(Y1) || e->bottom() > Y1)
        Y1 = e->bottom();
    }
  
  X0 = std::max(0.f, _scale * X0 - 10);
  Y0 = std::max(0.f, _scale * Y0 - 10);
  X1 = std::min((float)PageImage.cols, _scale * X1 + 10);
  Y1 = std::min((float)PageImage.rows, _scale * Y1 + 10);

  std::cerr << "Image with tag: " << ss.str() << std::endl;
  cv::imshow(ss.str(), PageImage(cv::Range(Y0, Y1), cv::Range(X0, X1)));
  cv::waitKey(0);
  cv::destroyWindow(ss.str());
}

void clsPdfLaDebug::setDebugOutputPath(const std::string& _debugOutputPath) {
  this->DebugOutputPath = _debugOutputPath;
}

}  // namespace PDFLA
}  // namespace Targoman

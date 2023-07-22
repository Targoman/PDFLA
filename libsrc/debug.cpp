#include "debug.h"

#ifndef EMSCRIPTEN
#include <opencv4/opencv2/opencv.hpp>
#include <sstream>
#endif

namespace Targoman {
namespace PDFLA {
using namespace Targoman::DLA;

struct stuPdfLaDebugData {
#ifndef EMSCRIPTEN
  std::map<size_t, RawImageData_t> PageImages;
  std::string ObjectBasename;
  size_t CurrentPageIndex;
#endif
};

clsPdfLaDebug::clsPdfLaDebug() {}

clsPdfLaDebug& clsPdfLaDebug::instance() {
  static std::unique_ptr<clsPdfLaDebug> Instance;
  if (__builtin_expect((Instance.get() == nullptr), 0))
    Instance.reset(new clsPdfLaDebug);
  return *Instance;
}

template <>
void clsPdfLaDebug::registerObject(const void* _object,
                                   const std::string& _basename) {
#ifndef EMSCRIPTEN
  if (this->DebugData.find(_object) == this->DebugData.end())
    this->DebugData[_object] = stuPdfLaDebugData{
        std::map<size_t, std::tuple<std::vector<uint8_t>, stuSize>>(),
        _basename, 0};
#endif
}

template <>
void clsPdfLaDebug::unregisterObject(const void* _object) {
#ifndef EMSCRIPTEN
  if (this->DebugData.find(_object) != this->DebugData.end())
    this->DebugData.erase(_object);
#endif
}

template <>
bool clsPdfLaDebug::isObjectRegistered(const void* _object) {
#ifndef EMSCRIPTEN
  return this->DebugData.find(_object) != this->DebugData.end();
#else
  return false;
#endif
}

template <>
bool clsPdfLaDebug::pageImageExists(const void* _object, size_t _pageIndex) {
#ifndef EMSCRIPTEN
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return false;
  auto& PageImages = DebugDataIterator->second.PageImages;
  return PageImages.find(_pageIndex) != PageImages.end();
#else
  return false;
#endif
}

#ifndef EMSCRIPTEN
cv::Mat bufferToMat(const std::vector<uint8_t>& _data, const stuSize& _size) {
  if (_data.size() == 0 || _size.area() < MIN_ITEM_SIZE) return cv::Mat();
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
#endif

template <>
void clsPdfLaDebug::registerPageImage(const void* _object, size_t _pageIndex,
                                      const std::vector<uint8_t>& _data,
                                      const stuSize& _size) {
#ifndef EMSCRIPTEN
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return;
  DebugDataIterator->second.PageImages[_pageIndex] =
      std::make_tuple(_data, _size);
#endif
}

template <>
const RawImageData_t& clsPdfLaDebug::getPageImage(const void* _object,
                                                  size_t _pageIndex) {
  static RawImageData_t Empty;
#ifndef EMSCRIPTEN
  using T = typename decltype(this->DebugData)::mapped_type;
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return Empty;
  auto& PageImages = DebugDataIterator->second.PageImages;
  auto PageImageIterator = PageImages.find(_pageIndex);
  if (PageImageIterator == PageImages.end()) return Empty;
  return PageImageIterator->second;
#else
  return Empty;
#endif
}

template <>
void clsPdfLaDebug::setCurrentPageIndex(const void* _object,
                                        size_t _pageIndex) {
#ifndef EMSCRIPTEN
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return;
  DebugDataIterator->second.CurrentPageIndex = _pageIndex;
#endif
}

#ifndef EMSCRIPTEN
cv::Rect bbox2CvRect(const stuBoundingBox& _bbox) {
  return cv::Rect(static_cast<int>(_bbox.left()), static_cast<int>(_bbox.top()),
                  static_cast<int>(_bbox.width()),
                  static_cast<int>(_bbox.height()));
}
#endif

void clsPdfLaDebug::setDebugOutputPath(const std::string& _debugOutputPath) {
#ifndef EMSCRIPTEN
  this->DebugOutputPath = _debugOutputPath;
#endif
}

#ifndef EMSCRIPTEN
static bool NoMorePdfLaDebugImages = false;
#endif

struct stuPdfLaDebugImageData {
#ifndef EMSCRIPTEN
  cv::Mat PageImage;
  std::string DebugOutputPath;
  std::string Basename;
  size_t PageIndex;
  stuBoundingBox RoiBounds;
  size_t ColorIndex;
  stuPdfLaDebugImageData(const RawImageData_t& _imageData,
                         const std::string& _debugOutputPath,
                         const std::string& _basename, size_t _pageIndex)
      : PageImage(
            bufferToMat(std::get<0>(_imageData), std::get<1>(_imageData))),
        DebugOutputPath(_debugOutputPath),
        Basename(_basename),
        PageIndex(_pageIndex),
        RoiBounds(static_cast<float>(PageImage.cols),
                  static_cast<float>(PageImage.rows), 0.f, 0.f),
        ColorIndex(0) {}
#endif
};

clsPdfLaDebugImage::clsPdfLaDebugImage(const RawImageData_t& _imageData,
                                       const std::string& _debugOutputPath,
                                       const std::string& _basename,
                                       size_t _pageIndex) {
#ifndef EMSCRIPTEN
  if (NoMorePdfLaDebugImages) return;
  this->Data.reset(new stuPdfLaDebugImageData(_imageData, _debugOutputPath,
                                              _basename, _pageIndex));
#endif
}

clsPdfLaDebugImage::clsPdfLaDebugImage() {}

#ifndef EMSCRIPTEN
constexpr int COLORS[][3] = {
    {0, 80, 0}, {0, 0, 255}, {80, 20, 150}, {0, 180, 180}};
constexpr size_t COLORS_SIZE = sizeof COLORS / sizeof COLORS[0];
#endif

clsPdfLaDebugImage& clsPdfLaDebugImage::add(
    const std::vector<const Targoman::DLA::stuBoundingBox*>& _boundingBoxes) {
#ifndef EMSCRIPTEN
  if (_boundingBoxes.empty()) return *this;
  if (this->Data.get() == nullptr) return *this;
  size_t ColorIndex = this->Data->ColorIndex++;
  this->Data->ColorIndex = (ColorIndex + 1) % COLORS_SIZE;

  cv::Scalar FillColor(COLORS[ColorIndex][0], COLORS[ColorIndex][1],
                       COLORS[ColorIndex][2]);
  cv::Scalar BorderColor(COLORS[ColorIndex][0] / 3, COLORS[ColorIndex][1] / 3,
                         COLORS[ColorIndex][2] / 3);
  for (auto& BoundingBox : _boundingBoxes) {
    if (BoundingBox == nullptr || BoundingBox->isEmpty()) continue;
    this->Data->RoiBounds.unionWith_(*BoundingBox);
    cv::Rect Rect = bbox2CvRect(BoundingBox->scale(DEBUG_UPSCALE_FACTOR));
    if (Rect.area() < 4) continue;
    cv::Mat ROI(this->Data->PageImage, Rect);
    cv::Mat RoiCopy = ROI.clone();
    cv::rectangle(RoiCopy, cv::Rect(0, 0, Rect.width, Rect.height), FillColor,
                  -1);
    cv::rectangle(RoiCopy, cv::Rect(3, 3, Rect.width - 6, Rect.height - 6),
                  BorderColor, 3);
    cv::addWeighted(ROI, 0.3, RoiCopy, 0.7, 0, ROI);

    cv::putText(this->Data->PageImage, std::to_string(ColorIndex),
                cv::Point(Rect.x, Rect.br().y), cv::FONT_HERSHEY_PLAIN, 3,
                cv::Scalar(0), 3);
  }
#endif
  return *this;
}

clsPdfLaDebugImage& clsPdfLaDebugImage::show(const std::string& _tag) {
#ifndef EMSCRIPTEN
  if (this->Data.get() == nullptr) return *this;
  if (this->Data->PageImage.empty()) return *this;

  std::ostringstream ss;
  ss << this->Data->Basename << "_p" << this->Data->PageIndex << std::setw(3)
     << "_" << _tag << ".png";

  stuBoundingBox Union = this->Data->RoiBounds;

  Union.scale_(DEBUG_UPSCALE_FACTOR);
  Union.inflate_(50);
  Union.intersectWith_(stuBoundingBox(0, 0, this->Data->PageImage.cols,
                                      this->Data->PageImage.rows));

  auto ROI = this->Data->PageImage(bbox2CvRect(Union));
  cv::Mat FinalImage;
  if (ROI.rows > 700 || ROI.cols > 1000) {
    double Scale = std::min((double)700 / ROI.rows, (double)1000 / ROI.cols);
    cv::resize(ROI, FinalImage, cv::Size(), Scale, Scale, cv::INTER_LANCZOS4);
  } else {
    ROI.assignTo(FinalImage);
  }

  std::cerr << "Image with tag: " << ss.str() << std::endl;
  cv::imshow(ss.str(), FinalImage);
  if (cv::waitKey(0) == 'Q') NoMorePdfLaDebugImages = true;
  cv::destroyWindow(ss.str());
#endif
  return *this;
}

clsPdfLaDebugImage& clsPdfLaDebugImage::save(const std::string& _tag) {
#ifndef EMSCRIPTEN
  if (this->Data.get() == nullptr) return *this;
  if (this->Data->PageImage.empty()) return *this;

  std::ostringstream ss;
  ss << this->Data->DebugOutputPath << "/" << this->Data->Basename << "_p"
     << std::setw(3) << this->Data->PageIndex << "_" << _tag << ".png";
  cv::imwrite(ss.str(), this->Data->PageImage);
#endif
  return *this;
}

template <>
clsPdfLaDebugImage clsPdfLaDebug::createImage(const void* _object) {
#ifndef EMSCRIPTEN
  if (NoMorePdfLaDebugImages) return clsPdfLaDebugImage();
  auto DebugDataIterator = this->DebugData.find(_object);
  if (DebugDataIterator == this->DebugData.end()) return clsPdfLaDebugImage();
  auto PageIndex = DebugDataIterator->second.CurrentPageIndex;
  auto PageImageDataIterator =
      DebugDataIterator->second.PageImages.find(PageIndex);
  if (PageImageDataIterator == DebugDataIterator->second.PageImages.end())
    return clsPdfLaDebugImage();
  return clsPdfLaDebugImage(
      PageImageDataIterator->second, this->DebugOutputPath,
      DebugDataIterator->second.ObjectBasename, PageIndex);
#else
  return clsPdfLaDebugImage();
#endif
}

}  // namespace PDFLA
}  // namespace Targoman


#include <pdfla/pdfla.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <opencv4/opencv2/opencv.hpp>
#include <string>

using namespace Targoman::DLA;
using namespace Targoman::PDFLA;

namespace fs = std::filesystem;

template <typename T0, typename Functor_t>
auto map(const std::vector<T0> &v, Functor_t f) {
  using T1 = decltype(std::declval<Functor_t>()(std::declval<T0>()));
  std::vector<T1> Result;
  Result.reserve(v.size());
  for (const auto &Item : v) Result.push_back(f(Item));
  return Result;
}

std::vector<uint8_t> readFileContents(const char *_filePath) {
  std::ifstream File(_filePath);
  File.seekg(0, std::ios_base::end);
  size_t FileSize = File.tellg();
  File.seekg(0, std::ios_base::beg);

  std::vector<uint8_t> FileContents(FileSize);
  File.read(reinterpret_cast<char *>(FileContents.data()), FileSize);

  return FileContents;
}

cv::Rect bbox2CvRect(const stuBoundingBox &_bbox) {
  return cv::Rect(static_cast<int>(_bbox.left()), static_cast<int>(_bbox.top()),
                  static_cast<int>(_bbox.width()),
                  static_cast<int>(_bbox.height()));
}

void processPdfFile(const std::string &_pdfFilePath, const std::string &_stem,
                    const std::string &_debugOut, const std::vector<size_t> _pageIndexes, bool _enableDebugging = false) {
  auto PdfFileContent = readFileContents(_pdfFilePath.data());
  auto PdfLa =
      std::make_shared<clsPdfLa>(PdfFileContent.data(), PdfFileContent.size());

  if(_enableDebugging)
    PdfLa->enableDebugging(fs::path(_pdfFilePath).stem());

  std::vector<size_t> PageIndexes = _pageIndexes;
  if(PageIndexes.size() == 0) {
    for (size_t PageIndex = 0; PageIndex < PdfLa->pageCount(); ++PageIndex)
      PageIndexes.push_back(PageIndex);
  }

  constexpr float Scale = 4;
  for (size_t PageIndex : PageIndexes) {
    auto Size = PdfLa->getPageSize(PageIndex);
    auto Blocks = PdfLa->getPageBlocks(PageIndex);
    Size = Size.scale(Scale);

    auto PageMatrixData = PdfLa->renderPageImage(PageIndex, 0xffffffff, Size);
    cv::Mat PageImage(Size.Height, Size.Width, CV_8UC3, PageMatrixData.data());
    for (auto &Block : Blocks) {
      auto Item = Block->BoundingBox;
      cv::Rect R = bbox2CvRect(Item.scale(Scale));
      
      if(R.area() < 4)
        continue;

      cv::Scalar FillColor;
      if (Block->Type == enuDocBlockType::Text)
        FillColor = cv::Scalar(100, 0, 0);
      else
        FillColor = cv::Scalar(0, 100, 0);

      auto ROI = PageImage(R);
      auto RoiCopy = ROI.clone();

      cv::rectangle(RoiCopy, cv::Rect(0, 0, R.width, R.height), FillColor, -1);
      cv::rectangle(RoiCopy, cv::Rect(0, 0, R.width, R.height), cv::Scalar(200, 200, 0), 3);
      cv::addWeighted(ROI, 0.3, RoiCopy, 0.7, 0, ROI);
    }

    std::ostringstream ss;
    ss << _debugOut << "/" << _stem << "_p" << std::setw(3) << PageIndex
       << ".png";
    cv::imwrite(ss.str(), PageImage);
    // cv::imshow(ss.str(), PageImage);
    // cv::waitKey(0);
  }
}

int main(void) {
  const std::string BasePath = "/data/Resources/Pdfs4LA/pdfs/col-2";
  const std::string DebugOutputPath =
      "/data/Work/Targoman/InternalProjects/TarjomyarV2/PDFA/debug";

  const std::vector<std::tuple<std::string, std::vector<size_t>>> ChosenPdfs{
      // { "bi-1097.pdf", {1, 5} },
      // { "bi-1121.pdf", { 0 } },
      // { "bi-1053.pdf" , { 0 } },
      // { "bi-1071.pdf", { 5 } },
      { "bi-1071.pdf", { 7 } },
      // { "bi-1248.pdf", { 0 } },
      // { "bi-1028.pdf", { 2 } },
      // { "bi-1023.pdf", { 1 } },
  };
  std::vector<std::tuple<fs::path, std::vector<size_t>>> PdfFilePaths;
  if (ChosenPdfs.size()) {
    PdfFilePaths = std::move(map(
        ChosenPdfs,
        [&BasePath](const std::tuple<std::string, std::vector<size_t>> &_item) {
          auto &[FileName, Pages] = _item;
          return std::make_tuple(fs::path(BasePath + "/" + FileName), Pages);
        }));
  } else {
    for (const auto &Entry : fs::directory_iterator(BasePath))
      if (Entry.path().extension() == ".pdf")
        PdfFilePaths.push_back(
            std::make_tuple(Entry.path(), std::vector<size_t>()));
  }

  setDebugOutputPath(DebugOutputPath);

  std::cout << "Searching `" << BasePath << "` ..." << std::endl;
  for (auto &[Path, Pages] : PdfFilePaths) {
      std::cout << Path.native() << std::endl;
      processPdfFile(Path, Path.stem(), DebugOutputPath, Pages, ChosenPdfs.size() > 0);
  }
  return 0;
}
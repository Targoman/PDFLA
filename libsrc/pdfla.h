#ifndef __TARGOMAN_PDFLA__
#define __TARGOMAN_PDFLA__

#include "dla.h"

namespace Targoman {
namespace PDFLA {

void setDebugOutputPath(const std::string& _path);

class clsPdfLaInternals;
class clsPdfLa {
 private:
  std::unique_ptr<clsPdfLaInternals> Internals;

 public:
  clsPdfLa(uint8_t *_data, size_t _size);
  ~clsPdfLa();

  size_t pageCount();

 public:
  Targoman::DLA::stuSize getPageSize(size_t _pageIndex);
  std::vector<uint8_t> renderPageImage(
      size_t _pageIndex, uint32_t _backgroundColor,
      const Targoman::DLA::stuSize &_renderSize);

 public:
  Targoman::DLA::DocBlockPtrVector_t getPageBlocks(size_t _pageIndex);

 public:
  void enableDebugging(const std::string &_basename);
};

}  // namespace PDFLA
}  // namespace Targoman

#endif  // __TARGOMAN_PDFLA__
#ifndef __TARGOMAN_PDFLA_CLSPDFIUMWRAPPER__
#define __TARGOMAN_PDFLA_CLSPDFIUMWRAPPER__

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Ignore PDFium warnings to stay vigilant about our own codes warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wattributes"
#include <fpdfapi/fpdf_module.h>
#include <fpdfapi/fpdf_page.h>
#include <fpdfapi/fpdfapi.h>
#include <fpdfdoc/fpdf_doc.h>
#include <fxcodec/fx_codec.h>
#include <fxcrt/fx_coordinates.h>
#pragma GCC diagnostic pop

#include "dla.h"

namespace Targoman {
namespace PDFLA {
constexpr float FONT_SIZE_UNIT = 1000.f;
constexpr float MAX_RGB_VALUE = 255.f;

class clsPdfFont {
 private:
  CPDF_Font *RawPdfFont;
  std::string FamilyName;

 public:
  clsPdfFont(CPDF_Font *_rawPdfFont);

  std::tuple<CFX_FloatRect, CFX_FloatRect> getGlyphBoxInfoForCode(
      FX_DWORD _charCode, float _fontSize) const;
  float getTypeAscent(float _fontSize) const;
  float getTypeDescent(float _fontSize) const;
  FX_DWORD getCodeFromUnicode(wchar_t _char) const;
  CFX_WideString getUnicodeFromCharCode(FX_DWORD _charCode) const;
  float getCharOffset(FX_DWORD _charCode, float _fontSize) const;
  float getCharAdvancement(FX_DWORD _charCode, float _fontSize) const;
  std::string familyName() const;
};

class clsPdfiumWrapper {
 private:
  std::shared_ptr<CPDF_Parser> Parser;
  std::map<size_t, std::shared_ptr<CPDF_Page>> LoadedPages;
  std::map<CPDF_Font *, std::shared_ptr<clsPdfFont>> LoadedFonts;

  std::shared_ptr<CPDF_Page> getPage(size_t _pageIndex);
  std::shared_ptr<clsPdfFont> getFont(CPDF_Font *_rawPdfFont);

 public:
  clsPdfiumWrapper(uint8_t *_data, size_t _size);

  size_t pageCount() const;
  Targoman::DLA::stuSize getPageSize(size_t _pageIndex);
  void populatePageObjects(CPDF_PageObjects *_source,
                           CPDF_PageObjects *_target);
  CPDF_PageObjects getPdfPageObjects(size_t _pageIndex);
  Targoman::DLA::DocItemPtrVector_t getPageItems(size_t _pageIndex);
  std::vector<uint8_t> renderPageImage(
      size_t _pageIndex, uint32_t _backgroundColor,
      const Targoman::DLA::stuSize &_renderSize);
};

}  // namespace PDFLA
}  // namespace Targoman

#endif  // __TARGOMAN_PDFLA_CLSPDFIUMWRAPPER__
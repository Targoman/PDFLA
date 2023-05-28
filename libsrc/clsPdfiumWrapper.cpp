#include "clsPdfiumWrapper.h"

namespace Targoman {
namespace PDFLA {

using namespace Targoman::DLA;

static bool __pdfiumModulesInitialized = false;
void initializePdfiumModules() {
  CPDF_ModuleMgr::Create();
  CFX_GEModule::Create();

  auto Codec = CCodec_ModuleMgr::Create();
  CFX_GEModule::Get()->SetCodecModule(Codec);
  CPDF_ModuleMgr::Get()->SetCodecModule(Codec);

  CPDF_ModuleMgr::Get()->InitPageModule();
  CPDF_ModuleMgr::Get()->InitRenderModule();
  CPDF_ModuleMgr::Get()->SetDownloadCallback(nullptr);
}

clsPdfFont::clsPdfFont(CPDF_Font *_rawPdfFont) : RawPdfFont(_rawPdfFont) {
  this->FamilyName = std::string(this->RawPdfFont->m_Font.GetFamilyName());
}

std::tuple<CFX_FloatRect, CFX_FloatRect> clsPdfFont::getGlyphBoxInfoForCode(
    FX_DWORD _charCode, float _fontSize) const {
  CFX_FloatRect CharBox, AdvanceBox;
  FX_RECT GlyphRect;
  this->RawPdfFont->GetCharBBox(_charCode, GlyphRect);

  CharBox.left = GlyphRect.left * _fontSize / FONT_SIZE_UNIT;
  CharBox.top = GlyphRect.top * _fontSize / FONT_SIZE_UNIT;
  CharBox.right = GlyphRect.right * _fontSize / FONT_SIZE_UNIT;
  CharBox.bottom = GlyphRect.bottom * _fontSize / FONT_SIZE_UNIT;

  AdvanceBox.left = 0;
  AdvanceBox.top = CharBox.top;
  AdvanceBox.right =
      this->RawPdfFont->GetCharWidthF(_charCode) * _fontSize / FONT_SIZE_UNIT;
  AdvanceBox.bottom = CharBox.bottom;

  return std::make_tuple(CharBox, AdvanceBox);
}

float clsPdfFont::getTypeAscent(float _fontSize) const {
  float Ascent = 0;
  for (wchar_t CharWithAscender :
       std::vector<wchar_t>{L'd', L'l', L'I', L'L'}) {
    FX_RECT RefBoundsRect;
    auto CharCode = this->RawPdfFont->CharCodeFromUnicode(CharWithAscender);
    if (CharCode != 0) {
      this->RawPdfFont->GetCharBBox(CharCode, RefBoundsRect);
      Ascent = RefBoundsRect.top * _fontSize / FONT_SIZE_UNIT;
      break;
    }
  }
  if (std::abs(Ascent) < MIN_ITEM_SIZE) {
    FX_RECT RefBoundsRect;
    this->RawPdfFont->GetFontBBox(RefBoundsRect);
    Ascent = RefBoundsRect.top * _fontSize / FONT_SIZE_UNIT;
  }
  Ascent = std::min(
      Ascent, this->RawPdfFont->GetTypeAscent() * _fontSize / FONT_SIZE_UNIT);
  return Ascent;
}

float clsPdfFont::getTypeDescent(float _fontSize) const {
  float Descent = 0;
  for (wchar_t CharWithDescender :
       std::vector<wchar_t>{L'g', L'j', L'p', L'q', L'y'}) {
    FX_RECT RefBoundsRect;
    auto CharCode = this->RawPdfFont->CharCodeFromUnicode(CharWithDescender);
    if (CharCode != 0) {
      this->RawPdfFont->GetCharBBox(CharCode, RefBoundsRect);
      Descent = RefBoundsRect.bottom * _fontSize / FONT_SIZE_UNIT;
      break;
    }
  }
  if (std::abs(Descent) < MIN_ITEM_SIZE) {
    FX_RECT RefBoundsRect;
    this->RawPdfFont->GetFontBBox(RefBoundsRect);
    Descent = RefBoundsRect.bottom * _fontSize / FONT_SIZE_UNIT;
  }
  Descent = std::max(
      Descent, this->RawPdfFont->GetTypeDescent() * _fontSize / FONT_SIZE_UNIT);
  return Descent;
}

FX_DWORD clsPdfFont::getCodeFromUnicode(wchar_t _char) const {
  return this->RawPdfFont->CharCodeFromUnicode(_char);
}

CFX_WideString clsPdfFont::getUnicodeFromCharCode(FX_DWORD _charCode) const {
  auto Result = this->RawPdfFont->UnicodeFromCharCode(_charCode);
  // if (Result.GetLength() == 0 || (Result.GetLength() == 1 && Result.GetAt(0)
  // < L' '))
  // {
  //     auto Replacement = this->findReplacement(_charCode);
  //     if (Replacement != 0)
  //     {
  //         if (Result.GetLength() == 0)
  //             Result.Insert(0, FX_WCHAR(Replacement));
  //         else
  //             Result.SetAt(0, FX_WCHAR(Replacement));
  //     }
  // }
  if (Result.GetLength() == 0) Result.Insert(0, FX_WCHAR(_charCode));
  return Result;
}

float clsPdfFont::getCharOffset(FX_DWORD _charCode, float _fontSize) const {
  FX_RECT BBox;
  this->RawPdfFont->GetCharBBox(_charCode, BBox);
  return BBox.left * _fontSize / FONT_SIZE_UNIT;
}

float clsPdfFont::getCharAdvancement(FX_DWORD _charCode,
                                     float _fontSize) const {
  return this->RawPdfFont->GetCharWidthF(_charCode) * _fontSize /
         FONT_SIZE_UNIT;
}

std::string clsPdfFont::familyName() const { return this->FamilyName; }

float getBrightness(CPDF_Color *_pdfColor) {
  if (_pdfColor == nullptr) return MAX_RGB_VALUE;
  int R, G, B;
  _pdfColor->GetRGB(R, G, B);
  return 0.299f * R + 0.587f * G + 0.114f * B;
}

struct CNode {
  CNode *pNext;
  CNode *pPrev;
  void *data;
};

std::shared_ptr<CPDF_Page> clsPdfiumWrapper::getPage(size_t _pageIndex) {
  if (this->LoadedPages.find(_pageIndex) != this->LoadedPages.end())
    return this->LoadedPages[_pageIndex];
  auto Page = std::make_shared<::CPDF_Page>();
  Page->Load(this->Parser->GetDocument(),
             this->Parser->GetDocument()->GetPage(_pageIndex));
  Page->ParseContent();
  this->LoadedPages[_pageIndex] = Page;
  return Page;
}

std::shared_ptr<clsPdfFont> clsPdfiumWrapper::getFont(CPDF_Font *_rawPdfFont) {
  if (this->LoadedFonts.find(_rawPdfFont) != this->LoadedFonts.end())
    return this->LoadedFonts[_rawPdfFont];
  auto Font = std::make_shared<clsPdfFont>(_rawPdfFont);
  this->LoadedFonts[_rawPdfFont] = Font;
  return Font;
}

clsPdfiumWrapper::clsPdfiumWrapper(uint8_t *_data, size_t _size) {
  if (!__glibc_likely(__pdfiumModulesInitialized)) {
    initializePdfiumModules();
    __pdfiumModulesInitialized = true;
  }
  this->Parser.reset(new CPDF_Parser);
  this->Parser->StartParse(FX_CreateMemoryStream(_data, _size));
}

size_t clsPdfiumWrapper::pageCount() const {
  return static_cast<size_t>(this->Parser->GetDocument()->GetPageCount());
}

stuSize clsPdfiumWrapper::getPageSize(size_t _pageIndex) {
  auto Page = this->getPage(_pageIndex);
  return stuSize(Page->GetPageWidth(), Page->GetPageHeight());
}

template <typename VisitText_t, typename VisitPath_t, typename VisitImage_t,
          typename Enter_t, typename Exit_t>
void traverseObjects(CPDF_FormObject *_pdfFormObject,
                     CPDF_PageObjects *_pageObjects, Enter_t _enter,
                     Exit_t _exit, VisitImage_t _visitImage,
                     VisitPath_t _visitPath, VisitText_t _visitText) {
  _enter(_pdfFormObject, _pageObjects);
  auto P = _pageObjects->GetFirstObjectPosition();
  while (P) {
    auto Object = _pageObjects->GetObjectAt(P);
    switch (Object ? Object->m_Type : 0) {
      case PDFPAGE_TEXT:
        _visitText(static_cast<CPDF_TextObject *>(Object));
        break;
      case PDFPAGE_PATH: {
        /* auto PathObject = static_cast<CPDF_PathObject *>(Object->Clone());
        auto OriginalPathData = PathObject->m_Path.GetObject();

        auto PointCount = OriginalPathData->GetPointCount();
        auto PointVector = OriginalPathData->GetPoints();

        auto CurrentPathData = PathObject->m_Path.New();
        CurrentPathData->SetPointCount(PointCount);

        int j = 0;
        for (int i = 0; i < PointCount; ++i)
        {
            if ((PointVector[i].m_Flag & FXPT_TYPE) == FXPT_MOVETO)
            {
                if (j > 0)
                {
                    CurrentPathData->SetPointCount(j);
                    _visitPath(PathObject);
                    auto Clone = static_cast<CPDF_PathObject
        *>(PathObject->Clone()); CurrentPathData = Clone->m_Path.New();
                    CurrentPathData->SetPointCount(PointCount - i);
                    j = 0;
                    delete PathObject;
                    PathObject = Clone;
                }
            }
            CurrentPathData->SetPoint(j, PointVector[i].m_PointX,
        PointVector[i].m_PointY, PointVector[i].m_Flag);
            ++j;
        }
        CurrentPathData->SetPointCount(j);
        _visitPath(PathObject);
        delete PathObject;
        */
        _visitPath(static_cast<CPDF_PathObject *>(Object));
      } break;
      case PDFPAGE_IMAGE:
        _visitImage(static_cast<CPDF_ImageObject *>(Object));
        break;
      case PDFPAGE_FORM:
        auto PdfFormObject = static_cast<CPDF_FormObject *>(Object);
        traverseObjects(PdfFormObject,
                        static_cast<CPDF_PageObjects *>(PdfFormObject->m_pForm),
                        _enter, _exit, _visitImage, _visitPath, _visitText);
        break;
    }
    P = static_cast<CNode *>(P)->pNext;
  }
  _exit();
}

CPDF_PageObjects clsPdfiumWrapper::getPdfPageObjects(size_t _pageIndex)

{
  CPDF_PageObjects Result;

  class clsPageObjectsWrapper {
   private:
    CPDF_PageObjects *RawPageObjects;
    void *TailPos;

   public:
    clsPageObjectsWrapper(CPDF_PageObjects *_rawPageObjects)
        : RawPageObjects(_rawPageObjects), TailPos(nullptr) {}

    void insert(CPDF_PageObject *_object) {
      this->TailPos =
          this->RawPageObjects->InsertObject(this->TailPos, _object);
    }
  };

  std::vector<clsPageObjectsWrapper> PageObjectHierarchy{
      clsPageObjectsWrapper(&Result)};
  auto Page = this->getPage(_pageIndex);
  traverseObjects(
      nullptr, Page.get(),
      [&](CPDF_FormObject *_formObject, CPDF_PageObjects *_pageObjects) {
        if (_formObject == nullptr) return;
        auto NewForm =
            _formObject->m_pForm == nullptr
                ? nullptr
                : new CPDF_Form(_formObject->m_pForm->m_pDocument,
                                _formObject->m_pForm->m_pPageResources,
                                _formObject->m_pForm->m_pFormStream,
                                _formObject->m_pForm->m_pPageResources);
        auto NewFormObject = new CPDF_FormObject();
        NewFormObject->m_FormMatrix = _formObject->m_FormMatrix;
        NewFormObject->m_pForm = NewForm;

        PageObjectHierarchy.back().insert(NewFormObject);
        PageObjectHierarchy.push_back(clsPageObjectsWrapper(NewForm));
      },
      [&]() { PageObjectHierarchy.pop_back(); },
      [&](CPDF_ImageObject *_imageObject) {
        PageObjectHierarchy.back().insert(_imageObject->Clone());
      },
      [&](CPDF_PathObject *_pathObject) {
        PageObjectHierarchy.back().insert(_pathObject->Clone());
      },
      [&](CPDF_TextObject *_textObject) {
        PageObjectHierarchy.back().insert(_textObject->Clone());
      });
  return Result;
}

bool isVerticalLine(const CPDF_PathObject *_pathObject, CFX_Matrix *_matrix) {
  auto BBox = _pathObject->GetBBox(_matrix);
  if (BBox.Width() < 4 && BBox.Height() > 4 * BBox.Width()) return true;
  return false;
}

bool isHorizontalLine(const CPDF_PathObject *_pathObject, CFX_Matrix *_matrix) {
  auto BBox = _pathObject->GetBBox(_matrix);
  if (BBox.Height() < 4 && BBox.Width() > 4 * BBox.Height()) return true;
  return false;
}

bool isSolidRectangle(const CPDF_PathObject *_pathObject, CFX_Matrix *_matrix) {
  auto PathData = _pathObject->m_Path.GetObject();
  auto N = PathData->GetPointCount();
  auto Points = PathData->GetPoints();

  auto NumberOfLegs = 1;
  auto PrevDX = 0, PrevDY = 0;

  FX_PATHPOINT *Origin = nullptr;
  FX_PATHPOINT *PrevPoint = nullptr;
  for (int i = 0; i < N; ++i) {
    if (PrevPoint == nullptr) {
      if ((Points[i].m_Flag & FXPT_TYPE) != FXPT_MOVETO) return false;
      PrevPoint = Points + i;
      Origin = PrevPoint;
      continue;
    }
    if ((Points[i].m_Flag & FXPT_TYPE) != FXPT_LINETO) return false;
    std::vector<FX_PATHPOINT *> CurrentPoints{Points + i};
    if ((Points[i].m_Flag & FXPT_CLOSEFIGURE) == FXPT_CLOSEFIGURE)
      CurrentPoints.push_back(Origin);
    for (auto &Point : CurrentPoints) {
      auto DX = std::abs(Points[i].m_PointX - PrevPoint->m_PointX);
      auto DY = std::abs(Points[i].m_PointY - PrevPoint->m_PointY);
      if (DX > MIN_ITEM_SIZE && DY > MIN_ITEM_SIZE) return false;
      if (DX <= MIN_ITEM_SIZE && DY <= MIN_ITEM_SIZE) continue;
      if ((DX <= MIN_ITEM_SIZE && PrevDX > MIN_ITEM_SIZE) ||
          (DY <= MIN_ITEM_SIZE && PrevDY > MIN_ITEM_SIZE))
        ++NumberOfLegs;
      if (NumberOfLegs > 4) return false;
      PrevPoint = Point;
      PrevDX = DX;
      PrevDY = DY;
    }
  }
  return NumberOfLegs == 4;
}

DocItemPtrVector_t clsPdfiumWrapper::getPageItems(size_t _pageIndex)

{
  auto Page = this->getPage(_pageIndex);

  auto PageBBox = Page->GetPageBBox();
  auto PdfiumPageMatrix = Page->GetPageMatrix();

  int PageRotation = 0;
  if (std::abs(PdfiumPageMatrix.a - 1.0f) > MIN_ITEM_SIZE) {
    if (std::abs(PdfiumPageMatrix.b - -1.0f) < MIN_ITEM_SIZE)
      PageRotation = 90;
    else if (std::abs(PdfiumPageMatrix.b - 0.f) < MIN_ITEM_SIZE)
      PageRotation = 180;
    else
      PageRotation = 270;
  }

  std::vector<std::shared_ptr<CFX_Matrix>> MatrixHierarchy;

  switch (PageRotation) {
    case 90:
      MatrixHierarchy.push_back(std::make_shared<CFX_Matrix>(
          0.f, 1.f, 1.f, 0.f, -PageBBox.bottom, -PageBBox.left));
      break;
    case 180:
      MatrixHierarchy.push_back(std::make_shared<CFX_Matrix>(
          -1.f, 0.f, 0.f, 1.f, PageBBox.right, -PageBBox.bottom));
      break;
    case 270:
      MatrixHierarchy.push_back(std::make_shared<CFX_Matrix>(
          0.f, -1.f, -1.f, 0.f, PageBBox.top, PageBBox.right));
      break;
    case 0:
    default:
      MatrixHierarchy.push_back(std::make_shared<CFX_Matrix>(
          1.f, 0.f, 0.f, -1.f, -PageBBox.left, PageBBox.top));
      break;
  }

  stuSize PageSize(Page->GetPageWidth(), Page->GetPageHeight());
  CFX_FloatRect PageRect(0, 0, PageSize.Width, PageSize.Height);
  DocItemPtrVector_t Result;

  auto appendFigureObject = [&](CPDF_PageObject *_object) {
    CFX_Matrix *TransformMatrix = MatrixHierarchy.back().get();
    CFX_FloatRect BoundingRect(_object->m_Left, _object->m_Bottom,
                               _object->m_Right, _object->m_Top);

    if (!_object->m_ClipPath.IsNull())
      BoundingRect.Intersect(_object->m_ClipPath.GetClipBox());

    if (TransformMatrix) TransformMatrix->TransformRect(BoundingRect);

    auto Type = enuDocItemType::Image;
    if (_object->m_Type == PDFPAGE_PATH) {
      Type = enuDocItemType::Path;
      auto PathObject = static_cast<CPDF_PathObject *>(_object);
      if (isVerticalLine(PathObject, TransformMatrix))
        Type = enuDocItemType::VerticalLine;
      else if (isHorizontalLine(PathObject, TransformMatrix))
        Type = enuDocItemType::HorizontalLine;
      else if (isSolidRectangle(PathObject, TransformMatrix))
        Type = enuDocItemType::SolidRectangle;
    }

    BoundingRect.Intersect(PageRect);
    stuBoundingBox BBox(BoundingRect.left, BoundingRect.bottom,
                        BoundingRect.right, BoundingRect.top);
    Result.push_back(
        std::make_shared<stuDocItem>(BBox, Type, NAN, NAN, NAN, 0));
  };

  auto appendTextObject = [&](CPDF_TextObject *_textObject) {
    CFX_Matrix *TransformMatrix = MatrixHierarchy.back().get();
    CFX_FloatRect WholeTextBoundRect(_textObject->m_Left, _textObject->m_Bottom,
                                     _textObject->m_Right, _textObject->m_Top);

    if (!_textObject->m_ClipPath.IsNull())
      WholeTextBoundRect.Intersect(_textObject->m_ClipPath.GetClipBox());

    if (abs(WholeTextBoundRect.Width()) < MIN_ITEM_SIZE ||
        abs(WholeTextBoundRect.Height()) < MIN_ITEM_SIZE)
      return;

    std::vector<FX_DWORD> FallbackCodes;
    std::vector<FX_FLOAT> FallbackPoses;
    int CharCount;
    FX_DWORD *CharCodes;
    FX_FLOAT *CharPoses;
    _textObject->GetData(CharCount, CharCodes, CharPoses);

    if (CharCount == 1) {
      FallbackCodes.push_back(
          static_cast<FX_DWORD>(reinterpret_cast<FX_UINTPTR>(CharCodes)));
      CharCodes = FallbackCodes.data();
    }

    if (CharPoses == nullptr) {
      FallbackPoses.resize(static_cast<size_t>(CharCount));
      CharPoses = FallbackPoses.data();
      _textObject->CalcCharPos(CharPoses);
      ++CharPoses;
    }

    CFX_AffineMatrix AffineMatrix;
    _textObject->GetTextMatrix(&AffineMatrix);
    if (TransformMatrix != nullptr) AffineMatrix.Concat(*TransformMatrix);

    float Angle = atan2(AffineMatrix.GetB(), AffineMatrix.GetA());
    auto Font = this->getFont(_textObject->GetFont());

    bool IsItalic = /* Font->isItalic() || */
        (std::abs(AffineMatrix.GetB()) > MIN_ITEM_SIZE &&
         std::abs(AffineMatrix.GetC()) < MIN_ITEM_SIZE) ||
        (std::abs(AffineMatrix.GetC()) > MIN_ITEM_SIZE &&
         std::abs(AffineMatrix.GetB()) < MIN_ITEM_SIZE);

    float FontSize = _textObject->GetFontSize();
    float BaseAscent = Font->getTypeAscent(FontSize),
          BaseDescent = Font->getTypeDescent(FontSize);

    float YUnit = AffineMatrix.GetYUnit();
    float ActualFontSize = YUnit * FontSize;

    auto allowedToHaveZeroSize = [](wchar_t _char) {
      return _char == L' ' || _char == L'\t';
    };

    int minIndex = 0, maxIndex = CharCount;

    auto shouldSkipThisCharCode = [&](const FX_DWORD &_charCode) {
      if (_charCode == static_cast<FX_DWORD>(-1)) return true;
      auto Unicode = Font->getUnicodeFromCharCode(_charCode);
      bool AllSpaces = true;
      for (int i = 0; i < Unicode.GetLength(); ++i)
        if (Unicode.GetAt(0) != L' ') {
          AllSpaces = false;
          break;
        }
      return AllSpaces;
    };

    //@NOTE:  pre/post text spaces are being removed because they are unreliable
    while (minIndex < maxIndex) {
      if (!shouldSkipThisCharCode(CharCodes[minIndex])) break;
      ++minIndex;
    }
    while (minIndex < maxIndex) {
      if (!shouldSkipThisCharCode(CharCodes[maxIndex - 1])) break;
      --maxIndex;
    }

    for (int i = minIndex; i < maxIndex; ++i) {
      FX_DWORD CharCode = CharCodes[i];
      if (CharCode == static_cast<FX_DWORD>(-1)) continue;

      CFX_FloatRect BoundingRect, AdvanceRect;

      std::tie(BoundingRect, AdvanceRect) =
          Font->getGlyphBoxInfoForCode(CharCode, FontSize);
      BoundingRect.left += i == 0 ? 0 : CharPoses[i - 1];
      BoundingRect.right += i == 0 ? 0 : CharPoses[i - 1];
      AdvanceRect.left += i == 0 ? 0 : CharPoses[i - 1];
      AdvanceRect.right += i == 0 ? 0 : CharPoses[i - 1];

      AffineMatrix.TransformRect(BoundingRect);
      AffineMatrix.TransformRect(AdvanceRect);

      if (BoundingRect.left >= PageSize.Width - MIN_ITEM_SIZE) continue;
      if (BoundingRect.bottom >= PageSize.Height - MIN_ITEM_SIZE) continue;
      if (BoundingRect.right < MIN_ITEM_SIZE) continue;
      if (BoundingRect.top < MIN_ITEM_SIZE) continue;

      float PosX;
      float Ascent = BaseAscent, Descent = BaseDescent, Baseline = 0;

      PosX = i == 0 ? 0 : CharPoses[i - 1];
      AffineMatrix.TransformPoint(PosX, Ascent);
      PosX = i == 0 ? 0 : CharPoses[i - 1];
      AffineMatrix.TransformPoint(PosX, Descent);
      PosX = i == 0 ? 0 : CharPoses[i - 1];
      AffineMatrix.TransformPoint(PosX, Baseline);

      auto Unicode = Font->getUnicodeFromCharCode(CharCode);
      if (Unicode.GetLength() == 0)
        Unicode.Insert(0, static_cast<wchar_t>(CharCode));

      BoundingRect.Intersect(PageRect);

      float X0 = BoundingRect.left;
      float WidthPerItem = BoundingRect.Width() / Unicode.GetLength();
      float O0 = AdvanceRect.left;
      float AdvancePerItem = AdvanceRect.Width() / Unicode.GetLength();

      for (int j = 0; j < Unicode.GetLength(); ++j) {
        if (allowedToHaveZeroSize(Unicode.GetAt(j)) ||
            (BoundingRect.Width() >= MIN_ITEM_SIZE &&
             BoundingRect.Height() >= MIN_ITEM_SIZE)) {
          Result.push_back(std::make_shared<stuDocItem>(
              stuBoundingBox(X0, BoundingRect.bottom, X0 + WidthPerItem,
                             BoundingRect.top),
              enuDocItemType::Char, Baseline,
              std::min(Ascent, BoundingRect.bottom),
              std::max(Descent, BoundingRect.top),
              static_cast<wchar_t>(Unicode.GetAt(j))));
        }
      }
    }
  };

  traverseObjects(
      nullptr, Page.get(),
      [&](CPDF_FormObject *_formObject, CPDF_PageObjects *_pageObjects) {
        if (_formObject == nullptr) return;
        auto NewMatrix =
            std::make_shared<CFX_Matrix>(_formObject->m_FormMatrix);
        NewMatrix->Concat(*MatrixHierarchy.back());
        MatrixHierarchy.push_back(NewMatrix);
      },
      [&]() { MatrixHierarchy.pop_back(); }, appendFigureObject,
      appendFigureObject, appendTextObject);

  return Result;
}

std::vector<uint8_t> clsPdfiumWrapper::renderPageImage(
    size_t _pageIndex, uint32_t _backgroundColor, const stuSize &_renderSize)

{
  std::vector<uint8_t> Buffer;
  auto Page = this->getPage(_pageIndex);

  int Width = static_cast<int>(_renderSize.Width);
  int Height = static_cast<int>(_renderSize.Height);
  Buffer.resize(Width * Height * 3);

  auto Bitmap = new ::CFX_DIBitmap;
  Bitmap->Create(Width, Height, FXDIB_Argb);
  FX_RECT WholePageRect(0, 0, Width, Height);

  CFX_FxgeDevice Device;
  Device.Attach(Bitmap);
  Device.FillRect(&WholePageRect, static_cast<FX_DWORD>(_backgroundColor));

  CFX_AffineMatrix Matrix;
  Page->GetDisplayMatrix(Matrix, 0, 0, Width, Height, 0);
  auto Context = new CPDF_RenderContext();
  Context->Create(Page.get());
  auto PageObjects = this->getPdfPageObjects(_pageIndex);
  Context->AppendObjectList(&PageObjects, &Matrix);
  auto Renderer = new CPDF_ProgressiveRenderer;
  Renderer->Start(Context, &Device, nullptr, nullptr);

  auto ResultScanLine = Buffer.data();
  for (int i = 0; i < Height; ++i) {
    auto ScanLine = Bitmap->GetScanline(i);
    for (int j = 0; j < Width; ++j) {
      ResultScanLine[0] = ScanLine[2];
      ResultScanLine[1] = ScanLine[1];
      ResultScanLine[2] = ScanLine[0];
      ResultScanLine += 3;
      ScanLine += 4;
    }
  }

  delete Renderer;
  delete Context;
  delete Bitmap;

  return Buffer;
}

}  // namespace PDFLA
}  // namespace Targoman
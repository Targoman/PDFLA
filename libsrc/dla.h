#ifndef __TARGOMAN_DLA__
#define __TARGOMAN_DLA__

#include <limits>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

namespace Targoman {
namespace DLA {
constexpr float MIN_ITEM_SIZE = 0.01f;

struct stuPoint {
  float X, Y;
  stuPoint(float _x, float _y) : X(_x), Y(_y) {}
  stuPoint() : stuPoint(0.f, 0.f) {}
  stuPoint scale(float _scale) const {
    return stuPoint(this->X * _scale, this->Y * _scale);
  }
};

struct stuSize {
  float Width;
  float Height;

  float area() const {
    return this->isEmpty() ? 0 : this->Width * this->Height;
  }

  bool isEmpty() const {
    return this->Width < MIN_ITEM_SIZE || this->Height < MIN_ITEM_SIZE;
  }

  stuSize(float _w = 0, float _h = 0) : Width(_w), Height(_h) {}
  stuSize scale(float _scale) const {
    return stuSize(this->Width * _scale, this->Height * _scale);
  }

  bool operator==(const stuSize &_other) const {
    return std::abs(this->Width - _other.Width) < MIN_ITEM_SIZE &&
           std::abs(this->Height - _other.Height) < MIN_ITEM_SIZE;
  }

  bool operator!= (const stuSize &_other) const {
    return !(this->operator== (_other));
  }
};

struct stuBoundingBox;
typedef std::shared_ptr<stuBoundingBox> BoundingBoxPtr_t;
struct stuBoundingBox {
  stuPoint Origin;
  stuSize Size;

  stuBoundingBox(const stuPoint &_origin, const stuSize &_size)
      : Origin(_origin), Size(_size) {}
  stuBoundingBox(float _x0, float _y0, float _x1, float _y1)
      : stuBoundingBox(stuPoint(_x0, _y0), stuSize(_x1 - _x0, _y1 - _y0)) {}
  stuBoundingBox(const stuPoint &_topLeft, const stuPoint &_bottomRight)
      : stuBoundingBox(_topLeft.X, _topLeft.Y, _bottomRight.X, _bottomRight.Y) {
  }
  stuBoundingBox() : stuBoundingBox(0.f, 0.f, 0.f, 0.f) {}

  float left() const { return this->Origin.X; }
  float top() const { return this->Origin.Y; }
  float right() const { return this->Origin.X + this->Size.Width; }
  float bottom() const { return this->Origin.Y + this->Size.Height; }

  float width() const { return this->Size.Width; }
  float height() const { return this->Size.Height; }

  float centerX() const { return this->Origin.X + this->Size.Width / 2; }
  float centerY() const { return this->Origin.Y + this->Size.Height / 2; }
  stuPoint center() const { return stuPoint(this->centerX(), this->centerY()); }

  float area() const { return this->Size.area(); }

  void unionWith_(const stuBoundingBox &_other);
  void unionWith_(const BoundingBoxPtr_t &_other);

  stuBoundingBox unionWith(const stuBoundingBox &_other) const;
  stuBoundingBox unionWith(const BoundingBoxPtr_t &_other) const;

  void intersectWith_(const stuBoundingBox &_other);
  void intersectWith_(const BoundingBoxPtr_t &_other);

  stuBoundingBox intersectWith(const stuBoundingBox &_other) const;
  stuBoundingBox intersectWith(const BoundingBoxPtr_t &_other) const;

  bool hasIntersectionWith(const stuBoundingBox &_other) const;
  bool hasIntersectionWith(const BoundingBoxPtr_t &_other) const;

  void minus_(const stuBoundingBox &_other);
  void minus_(const BoundingBoxPtr_t &_other);

  stuBoundingBox minus(const stuBoundingBox &_other) const;
  stuBoundingBox minus(const BoundingBoxPtr_t &_other) const;

  float horizontalOverlap(const stuBoundingBox &_other) const;
  float horizontalOverlap(const BoundingBoxPtr_t &_other) const;

  float verticalOverlap(const stuBoundingBox &_other) const;
  float verticalOverlap(const BoundingBoxPtr_t &_other) const;

  float horizontalOverlapRatio(const stuBoundingBox &_other) const;
  float horizontalOverlapRatio(const BoundingBoxPtr_t &_other) const;

  float verticalOverlapRatio(const stuBoundingBox &_other) const;
  float verticalOverlapRatio(const BoundingBoxPtr_t &_other) const;

  bool isHorizontalRuler() const;
  bool isVerticalRuler() const;

  bool isEmpty() const;

  bool contains(const stuBoundingBox &_other) const;
  bool contains(const BoundingBoxPtr_t &_other) const;

  stuBoundingBox scale(float _scale) const {
    return stuBoundingBox(this->Origin.scale(_scale), this->Size.scale(_scale));
  }
};
typedef std::vector<BoundingBoxPtr_t> BoundingBoxPtrVector_t;

enum class enuDocItemType {
  None,
  Image,
  Path,
  VerticalLine,
  HorizontalLine,
  SolidRectangle,
  Char,
  Background
};

enum class enuDocArea {
  Header,
  Body,
  Footer,
  LeftSidebar,
  RightSidebar,
  Watermark
};

struct stuDocItem {
  stuBoundingBox BoundingBox;
  enuDocItemType Type;
  int32_t RepetitionPageOffset;
  float Baseline, Ascent, Descent;
  wchar_t Char;
  stuDocItem(const stuBoundingBox &_boundingBox, enuDocItemType _type,
             float _baseline, float _ascent, float _descent, wchar_t _char)
      : BoundingBox(_boundingBox),
        Type(_type),
        Baseline(_baseline),
        Ascent(_ascent),
        Descent(_descent),
        Char(_char) {}
};
typedef std::shared_ptr<stuDocItem> DocItemPtr_t;
typedef std::vector<DocItemPtr_t> DocItemPtrVector_t;

enum class enuDocBlockType { Text, Figure, Table, Formulae };

struct stuDocBlock {
  stuBoundingBox BoundingBox;
  enuDocArea Area;
  enuDocBlockType Type;
  DocItemPtrVector_t Elements;
  stuDocBlock(enuDocBlockType _type) : Type(_type) {}
};

struct stuDocTextBlock;
struct stuDocFigureBlock;
struct stuDocTableBlock;
struct stuDocFormulaeBlock;
class clsDocBlockPtr : public std::shared_ptr<stuDocBlock> {
 public:
  inline stuDocTextBlock *asText();
  inline stuDocFigureBlock *asFigure();
  inline stuDocTableBlock *asTable();
  inline stuDocFormulaeBlock *asFormulae();
};

typedef std::vector<clsDocBlockPtr> DocBlockPtrVector_t;

enum class enuListType { None, Bulleted, Numbered };

struct stuDocLine {
  stuBoundingBox BoundingBox;
  float Baseline;
  int32_t ID;
  enuListType ListType;
  float TextLeft;
  DocItemPtrVector_t Items;
};
typedef std::shared_ptr<stuDocLine> DocLinePtr_t;
typedef std::vector<DocLinePtr_t> DocLinePtrVector_t;

enum class enuDocTextBlockAssociation { None, IsCaptionOf, IsInsideOf };
struct stuDocTextBlock : public stuDocBlock {
  DocLinePtrVector_t Lines;
  enuDocTextBlockAssociation Association;
  clsDocBlockPtr AssociatedBlock;
  stuDocTextBlock() : stuDocBlock(enuDocBlockType::Text) {}
};

struct stuDocFigureBlock : public stuDocBlock {
  clsDocBlockPtr Caption;
  stuDocFigureBlock() : stuDocBlock(enuDocBlockType::Figure) {}
};

struct stuDocTableCell {
  clsDocBlockPtr Text;
  int16_t Row, RowSpan;
  int16_t Col, ColSpan;
};
typedef std::vector<stuDocTableCell> stuDocTableCellVector_t;
struct stuDocTableBlock : public stuDocBlock {
  clsDocBlockPtr Caption;
  stuDocTableCellVector_t Cells;
  stuDocTableBlock() : stuDocBlock(enuDocBlockType::Table) {}
};

struct stuDocFormulaeBlock : public stuDocBlock {
  std::wstring LatexSource;
  stuDocFormulaeBlock() : stuDocBlock(enuDocBlockType::Formulae) {}
};

inline stuDocTextBlock *clsDocBlockPtr::asText() {
  return static_cast<stuDocTextBlock *>(this->get());
}
inline stuDocFigureBlock *clsDocBlockPtr::asFigure() {
  return static_cast<stuDocFigureBlock *>(this->get());
}
inline stuDocTableBlock *clsDocBlockPtr::asTable() {
  return static_cast<stuDocTableBlock *>(this->get());
}
inline stuDocFormulaeBlock *clsDocBlockPtr::asFormulae() {
  return static_cast<stuDocFormulaeBlock *>(this->get());
}

}  // namespace DLA
}  // namespace Targoman

#endif  // __TARGOMAN_DLA__
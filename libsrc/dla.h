#ifndef __TARGOMAN_DLA__
#define __TARGOMAN_DLA__

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "type_traits.hpp"

namespace Targoman {
namespace DLA {
constexpr float MIN_ITEM_SIZE = 0.1f;

struct stuPoint {
  float X, Y;
  stuPoint(float _x, float _y) : X(_x), Y(_y) {}
  stuPoint() : stuPoint(0.f, 0.f) {}
  void scale_(float _scale) {
    this->X *= _scale;
    this->Y *= _scale;
  }
  stuPoint scale(float _scale) const {
    auto Copy = *this;
    Copy.scale(_scale);
    return Copy;
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

  void scale_(float _scale) {
    this->Width *= _scale;
    this->Height *= _scale;
  }

  stuSize scale(float _scale) const {
    auto Copy = *this;
    Copy.scale_(_scale);
    return Copy;
  }

  bool operator==(const stuSize &_other) const {
    return std::abs(this->Width - _other.Width) < MIN_ITEM_SIZE &&
           std::abs(this->Height - _other.Height) < MIN_ITEM_SIZE;
  }

  bool operator!=(const stuSize &_other) const {
    return !(this->operator==(_other));
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

  void scale_(float _scale) {
    this->Origin.scale_(_scale);
    this->Size.scale_(_scale);
  }

  stuBoundingBox scale(float _scale) const {
    auto Copy = *this;
    Copy.scale_(_scale);
    return Copy;
  }

  void inflate_(float _amount) {
    this->Origin.X -= _amount;
    this->Origin.Y -= _amount;
    this->Size.Width += 2 * _amount;
    this->Size.Height += 2 * _amount;
  }

  stuBoundingBox inflate(float _amount) const {
    auto Copy = *this;
    Copy.inflate_(_amount);
    return Copy;
  }
};
typedef std::vector<BoundingBoxPtr_t> BoundingBoxPtrVector_t;

enum class enuBoundingBoxOrdering { L2R, T2B, L2RT2B, T2BL2R };

struct stuBoxBoundedItem {
  stuBoundingBox BoundingBox;
  stuBoxBoundedItem() {}
  stuBoxBoundedItem(const stuBoundingBox &_boundingBox)
      : BoundingBox(_boundingBox) {}
};

template <enuBoundingBoxOrdering O>
struct stuBoundingBoxComparatorImpl {};

template <>
struct stuBoundingBoxComparatorImpl<enuBoundingBoxOrdering::L2R> {
  bool operator()(const stuBoundingBox &a, const stuBoundingBox &b) {
    return a.left() < b.left();
  }
};

template <>
struct stuBoundingBoxComparatorImpl<enuBoundingBoxOrdering::L2RT2B> {
  bool operator()(const stuBoundingBox &a, const stuBoundingBox &b) {
    if (a.horizontalOverlap(b) > MIN_ITEM_SIZE) return a.top() < b.top();
    return a.left() < b.left();
  }
};

template <>
struct stuBoundingBoxComparatorImpl<enuBoundingBoxOrdering::T2B> {
  bool operator()(const stuBoundingBox &a, const stuBoundingBox &b) {
    return a.top() < b.top();
  }
};

template <>
struct stuBoundingBoxComparatorImpl<enuBoundingBoxOrdering::T2BL2R> {
  bool operator()(const stuBoundingBox &a, const stuBoundingBox &b) {
    if (a.verticalOverlap(b) > MIN_ITEM_SIZE) return a.left() < b.left();
    return a.top() < b.top();
  }
};

template <typename T, enuBoundingBoxOrdering O, typename = void>
struct stuBoundingBoxComparator {};

template <typename T, enuBoundingBoxOrdering O>
struct stuBoundingBoxComparator<
    T, O,
    typename Targoman::Common::stuContainerTypeTraits<
        T>::stuOnlyForPointers::type> {
  using U = typename Targoman::Common::stuContainerTypeTraits<T>::ElementType_t;
  using V = typename Targoman::Common::stuSharedPtrTypeTraits<
      typename Targoman::Common::stuContainerTypeTraits<T>::ElementBareType_t>::
      ElementType_t;
  bool operator()(const U &a, const U &b) {
    return stuBoundingBoxComparator<std::vector<V>, O>()(*a, *b);
  }
};

template <typename T, enuBoundingBoxOrdering O>
struct stuBoundingBoxComparator<
    T, O,
    typename Targoman::Common::stuContainerTypeTraits<T>::stuOnlyForNonPointers<
        stuBoxBoundedItem>::type> {
  using U = typename Targoman::Common::stuContainerTypeTraits<T>::ElementType_t;
  bool operator()(const U &a, const U &b) {
    return stuBoundingBoxComparatorImpl<O>()(a.BoundingBox, b.BoundingBox);
  }
};

template <typename T, enuBoundingBoxOrdering O>
struct stuBoundingBoxComparator<
    T, O,
    typename Targoman::Common::stuContainerTypeTraits<T>::OnlyForNonPointers_t<
        stuBoundingBox>> {
  using U = typename Targoman::Common::stuContainerTypeTraits<T>::ElementType_t;
  bool operator()(const U &a, const U &b) {
    return stuBoundingBoxComparatorImpl<O>()(a, b);
  }
};

template <enuBoundingBoxOrdering O, typename T>
void sortByBoundingBoxes(T &_container) {
  std::sort(_container.begin(), _container.end(),
            stuBoundingBoxComparator<T, O>());
}

template <enuBoundingBoxOrdering O, typename T>
T sortedByBoundingBoxes(const T &_container) {
  T Copy = _container;
  sortByBoundingBoxes<O, T>(Copy);
  return Copy;
}

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

struct stuDocItem : public stuBoxBoundedItem {
  enuDocItemType Type;
  int32_t RepetitionPageOffset;
  float Baseline, Ascent, Descent, BaselineAngle;
  wchar_t Char;
  stuDocItem(const stuBoundingBox &_boundingBox, enuDocItemType _type,
             float _baseline, float _ascent, float _descent,
             float _baselineAngle, wchar_t _char)
      : stuBoxBoundedItem(_boundingBox),
        Type(_type),
        Baseline(_baseline),
        Ascent(_ascent),
        Descent(_descent),
        BaselineAngle(_baselineAngle),
        Char(_char) {}
  stuBoundingBox BoundsInLine() const {
    if (this->Type != enuDocItemType::Char) return this->BoundingBox;
    return stuBoundingBox(this->BoundingBox.left(), this->Ascent,
                          this->BoundingBox.right(), this->Descent);
  }
};
typedef std::shared_ptr<stuDocItem> DocItemPtr_t;
typedef std::vector<DocItemPtr_t> DocItemPtrVector_t;

enum class enuDocBlockType { Text, Figure, Table, Formulae };

struct stuDocBlock : public stuBoxBoundedItem {
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

  inline const stuDocTextBlock *asText() const;
  inline const stuDocFigureBlock *asFigure() const;
  inline const stuDocTableBlock *asTable() const;
  inline const stuDocFormulaeBlock *asFormulae() const;
};

typedef std::vector<clsDocBlockPtr> DocBlockPtrVector_t;

enum class enuListType { None, Bulleted, Numbered };

struct stuDocLine;
typedef std::shared_ptr<stuDocLine> DocLinePtr_t;
typedef std::vector<DocLinePtr_t> DocLinePtrVector_t;
struct stuDocLine : public stuBoxBoundedItem {
  float Baseline;
  int32_t ID;
  enuListType ListType;
  float TextLeft;
  DocItemPtrVector_t Items;

  void mergeWith_(const stuDocLine &_otherLine);
  void mergeWith_(const DocLinePtr_t &_otherLine);
};

enum class enuDocTextBlockAssociation { None, IsCaptionOf, IsInsideOf };
struct stuDocTextBlock : public stuDocBlock {
  DocLinePtrVector_t Lines;
  enuDocTextBlockAssociation Association;
  clsDocBlockPtr AssociatedBlock;
  stuDocTextBlock() : stuDocBlock(enuDocBlockType::Text) {}
  void mergeWith_(const stuDocTextBlock &_otherBlock);
  void mergeWith_(const clsDocBlockPtr &_otherBlock);
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

inline const stuDocTextBlock *clsDocBlockPtr::asText() const {
  return static_cast<stuDocTextBlock const *>(this->get());
}
inline const stuDocFigureBlock *clsDocBlockPtr::asFigure() const {
  return static_cast<stuDocFigureBlock const *>(this->get());
}
inline const stuDocTableBlock *clsDocBlockPtr::asTable() const {
  return static_cast<stuDocTableBlock const *>(this->get());
}
inline const stuDocFormulaeBlock *clsDocBlockPtr::asFormulae() const {
  return static_cast<stuDocFormulaeBlock const *>(this->get());
}

}  // namespace DLA
}  // namespace Targoman

#endif  // __TARGOMAN_DLA__
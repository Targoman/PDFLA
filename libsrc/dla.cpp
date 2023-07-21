#include "dla.h"

#include <assert.h>

#include <set>
#include <sstream>

#include "algorithm.hpp"

namespace Targoman {
namespace DLA {

using namespace Targoman::Common;

constexpr float MAX_RULER_THIN_SIZE = 4.f;
constexpr float MIN_RULER_THICK_SIZE = 8.f;
constexpr float MIN_RULER_ASPECT_RATIO = 4.f;

void stuBoundingBox::unionWith_(const stuBoundingBox &_other) {
  float X0 = std::min(this->left(), _other.left());
  float Y0 = std::min(this->top(), _other.top());
  float X1 = std::max(this->right(), _other.right());
  float Y1 = std::max(this->bottom(), _other.bottom());
  *this = stuBoundingBox(X0, Y0, X1, Y1);
}

void stuBoundingBox::unionWith_(const BoundingBoxPtr_t &_other) {
  this->unionWith_(*_other);
}

void stuBoundingBox::unionWithInDirection_(const stuBoundingBox &_other,
                                           bool _horizontally) {
  auto unionWithInDirectionImpl = [](float &_p0, float &_size,
                                     const float &_otherP0,
                                     const float &_otherSize) {
    float minP = std::min(_p0, _otherP0);
    float maxP = std::max(_p0 + _size, _otherP0 + _otherSize);
    _p0 = minP;
    _size = maxP - minP;
  };
  if (_horizontally)
    unionWithInDirectionImpl(this->Origin.X, this->Size.Width, _other.Origin.X,
                             _other.Size.Width);
  else
    unionWithInDirectionImpl(this->Origin.Y, this->Size.Height, _other.Origin.Y,
                             _other.Size.Height);
}

void stuBoundingBox::unionWithInDirection_(const BoundingBoxPtr_t &_other,
                                           bool _horizontally) {
  this->unionWithInDirection_(*_other, _horizontally);
}

stuBoundingBox stuBoundingBox::unionWith(const stuBoundingBox &_other) const {
  stuBoundingBox Union = *this;
  Union.unionWith_(_other);
  return Union;
}

stuBoundingBox stuBoundingBox::unionWith(const BoundingBoxPtr_t &_other) const {
  return this->unionWith(*_other);
}

stuBoundingBox stuBoundingBox::unionWithInDirection(
    const stuBoundingBox &_other, bool _horizontally) {
  auto Copy = *this;
  Copy.unionWithInDirection_(_other, _horizontally);
  return Copy;
}

stuBoundingBox stuBoundingBox::unionWithInDirection(
    const BoundingBoxPtr_t &_other, bool _horizontally) {
  return this->unionWithInDirection(*_other, _horizontally);
}

void stuBoundingBox::intersectWith_(const stuBoundingBox &_other) {
  float X0 = std::max(this->left(), _other.left());
  float Y0 = std::max(this->top(), _other.top());
  float X1 = std::min(this->right(), _other.right());
  float Y1 = std::min(this->bottom(), _other.bottom());
  *this = stuBoundingBox(X0, Y0, X1, Y1);
}

void stuBoundingBox::intersectWith_(const BoundingBoxPtr_t &_other) {
  this->intersectWith_(*_other);
}

stuBoundingBox stuBoundingBox::intersectWith(
    const stuBoundingBox &_other) const {
  stuBoundingBox Intersection = *this;
  Intersection.intersectWith_(_other);
  return Intersection;
}

stuBoundingBox stuBoundingBox::intersectWith(
    const BoundingBoxPtr_t &_other) const {
  return this->intersectWith(*_other);
}

bool stuBoundingBox::hasIntersectionWith(const stuBoundingBox &_other) const {
  auto ThresholdX =
      std::min(std::min(this->width(), _other.width()), MIN_ITEM_SIZE);
  auto ThresholdY =
      std::min(std::min(this->height(), _other.height()), MIN_ITEM_SIZE);
  return this->horizontalOverlap(_other) >= ThresholdX &&
         this->verticalOverlap(_other) >= ThresholdY;
}

bool stuBoundingBox::hasIntersectionWith(const BoundingBoxPtr_t &_other) const {
  return this->hasIntersectionWith(*_other);
}

void stuBoundingBox::minus_(const stuBoundingBox &_other) {
  if (_other.left() < this->left() + MIN_ITEM_SIZE &&
      _other.right() > this->right() - MIN_ITEM_SIZE) {
    if (_other.top() < this->top() + MIN_ITEM_SIZE)
      this->Origin.Y = _other.bottom();
    if (_other.bottom() > this->bottom() - MIN_ITEM_SIZE)
      this->Size.Height = _other.top() - this->top();
  }
  if (_other.top() < this->top() + MIN_ITEM_SIZE &&
      _other.bottom() > this->bottom() - MIN_ITEM_SIZE) {
    if (_other.left() < this->left() + MIN_ITEM_SIZE)
      this->Origin.X = _other.right();
    if (_other.right() > this->right() - MIN_ITEM_SIZE)
      this->Size.Width = _other.left() - this->left();
  }
  if (this->isEmpty()) {
    this->Origin = stuPoint(0.f, 0.f);
    this->Size = stuSize(0.f, 0.f);
  }
}

void stuBoundingBox::minus_(const BoundingBoxPtr_t &_other) {
  this->minus_(*_other);
}

stuBoundingBox stuBoundingBox::minus(const stuBoundingBox &_other) const {
  auto Copy = *this;
  Copy.minus_(_other);
  return Copy;
}

stuBoundingBox stuBoundingBox::minus(const BoundingBoxPtr_t &_other) const {
  return this->minus(*_other);
}

float stuBoundingBox::horizontalOverlap(const stuBoundingBox &_other) const {
  float X0 = std::max(this->left(), _other.left());
  float X1 = std::min(this->right(), _other.right());
  return X1 - X0;
}

float stuBoundingBox::horizontalOverlap(const BoundingBoxPtr_t &_other) const {
  return this->horizontalOverlap(*_other);
}

float stuBoundingBox::verticalOverlap(const stuBoundingBox &_other) const {
  float Y0 = std::max(this->top(), _other.top());
  float Y1 = std::min(this->bottom(), _other.bottom());
  return Y1 - Y0;
}

float stuBoundingBox::verticalOverlap(const BoundingBoxPtr_t &_other) const {
  return this->verticalOverlap(*_other);
}

float stuBoundingBox::horizontalOverlapRatio(
    const stuBoundingBox &_other) const {
  if (this->isEmpty() || _other.isEmpty()) return 0.0f;
  return this->horizontalOverlap(_other) /
         std::min(this->width(), _other.width());
}

float stuBoundingBox::horizontalOverlapRatio(
    const BoundingBoxPtr_t &_other) const {
  return this->horizontalOverlapRatio(*_other);
}

float stuBoundingBox::verticalOverlapRatio(const stuBoundingBox &_other) const {
  if (this->isEmpty() || _other.isEmpty()) return 0.0f;
  return this->verticalOverlap(_other) /
         std::min(this->height(), _other.height());
}

float stuBoundingBox::verticalOverlapRatio(
    const BoundingBoxPtr_t &_other) const {
  return this->verticalOverlapRatio(*_other);
}

bool stuBoundingBox::isHorizontalRuler() const {
  return (this->height() < MAX_RULER_THIN_SIZE) &&
         this->width() > std::max(MIN_RULER_THICK_SIZE,
                                  MIN_RULER_ASPECT_RATIO * this->height());
}

bool stuBoundingBox::isVerticalRuler() const {
  return (this->width() < MAX_RULER_THIN_SIZE) &&
         this->height() > std::max(MIN_RULER_THICK_SIZE,
                                   MIN_RULER_ASPECT_RATIO * this->width());
}

bool stuBoundingBox::isEmpty() const { return this->Size.isEmpty(); }

bool stuBoundingBox::contains(const stuBoundingBox &_other) const {
  return _other.left() >= this->left() && _other.right() <= this->right() &&
         _other.top() >= this->top() && _other.bottom() <= this->bottom();
}

bool stuBoundingBox::contains(const BoundingBoxPtr_t &_other) const {
  return this->contains(*_other);
}

bool stuBoundingBox::isStronglyLeftOf(const stuBoundingBox &_other) const {
  return this->left() < _other.left() && this->right() < _other.right();
}

bool stuBoundingBox::isStronglyLeftOf(const BoundingBoxPtr_t &_other) const {
  return this->isStronglyLeftOf(*_other);
}

bool stuBoundingBox::isStronglyRightOf(const stuBoundingBox &_other) const {
  return this->left() > _other.left() && this->right() > _other.right();
}

bool stuBoundingBox::isStronglyRightOf(const BoundingBoxPtr_t &_other) const {
  return this->isStronglyRightOf(*_other);
}

void stuDocLine::mergeWith_(const stuDocLine &_otherLine) {
  this->BoundingBox.unionWith_(_otherLine.BoundingBox);
  this->Items.insert(this->Items.end(), _otherLine.Items.begin(),
                     _otherLine.Items.end());
}

void stuDocLine::mergeWith_(const DocLinePtr_t &_otherLine) {
  return this->mergeWith_(*_otherLine);
}

void stuDocLine::computeBaseline() {
  if (std::isnan(this->Baseline)) {
    auto ItemBaselines =
        map(this->Items, [](const DocItemPtr_t &e) { return e->Baseline; });
    auto [Mean, Std] = meanAndStd(ItemBaselines);
    ItemBaselines.erase(
        std::remove_if(ItemBaselines.begin(), ItemBaselines.end(),
                       [&Mean = Mean, &Std = Std](float v) {
                         return std::abs(v - Mean) <= Std;
                       }),
        ItemBaselines.end());
    this->Baseline = mean(ItemBaselines);
  }
}

float stuDocLine::baselineDifference(const stuDocLine &_otherLine) {
  return this->Baseline - _otherLine.Baseline;
}

float stuDocLine::baselineDifference(const DocLinePtr_t &_otherLine) {
  return this->baselineDifference(*_otherLine);
}

float stuDocLine::overlap(const stuDocLine &_otherLine) const {
  float Y0 = std::max(
      min(this->Items, [](const DocItemPtr_t &e) { return e->Ascent; }),
      min(_otherLine.Items, [](const DocItemPtr_t &e) { return e->Ascent; }));
  float Y1 = std::min(
      max(this->Items, [](const DocItemPtr_t &e) { return e->Descent; }),
      max(_otherLine.Items, [](const DocItemPtr_t &e) { return e->Descent; }));
  return Y1 - Y0;
}

float stuDocLine::overlap(const DocLinePtr_t &_otherLine) const {
  return this->overlap(*_otherLine);
}

std::u32string stuDocLine::contents() const {
  if (this->Items.empty()) return std::u32string();
  std::u32string Result;
  Result.reserve(this->Items.size() * 2);
  size_t i = 0;
  while (i < this->Items.size()) {
    if (i < this->Items.size() - 1 &&
        this->Items[i]->BoundingBox.isStronglyLeftOf(
            this->Items[i + 1]->BoundingBox)) {
      Result += U" __FORMULAE__ ";
      ++i;
      while (i < this->Items.size() - 1 &&
             !this->Items[i]->BoundingBox.isStronglyLeftOf(
                 this->Items[i + 1]->BoundingBox))
        ++i;
      if (i > 0 && i == this->Items.size() - 1 &&
          !this->Items[i]->BoundingBox.isStronglyRightOf(
              this->Items[i - 1]->BoundingBox))
        ++i;
    } else {
      Result.push_back(this->Items[i]->Char);
      ++i;
    }
  }
  Result.shrink_to_fit();
  return Result;
}

void stuDocTextBlock::mergeWith_(const stuDocTextBlock &_otherBlock) {
  this->BoundingBox.unionWith_(_otherBlock.BoundingBox);
  this->Lines.insert(this->Lines.end(), _otherBlock.Lines.begin(),
                     _otherBlock.Lines.end());
}

void stuDocTextBlock::mergeWith_(const clsDocBlockPtr &_otherBlock) {
  if (_otherBlock->Type != enuDocBlockType::Text) return;
  this->mergeWith_(*(_otherBlock.asText()));
}

}  // namespace DLA
}  // namespace Targoman
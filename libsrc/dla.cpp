#include "dla.h"

#include <assert.h>

#include <set>
#include <sstream>

namespace Targoman {
namespace DLA {

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

stuBoundingBox stuBoundingBox::unionWith(const stuBoundingBox &_other) const {
  stuBoundingBox Union = *this;
  Union.unionWith_(_other);
  return Union;
}

stuBoundingBox stuBoundingBox::unionWith(const BoundingBoxPtr_t &_other) const {
  return this->unionWith(*_other);
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
  return this->horizontalOverlap(_other) > MIN_ITEM_SIZE &&
         this->verticalOverlap(_other) > MIN_ITEM_SIZE;
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

}  // namespace DLA
}  // namespace Targoman
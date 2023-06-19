#include "pdfla.h"

#include <iostream>
#include <set>

#include "algorithm.hpp"
#include "clsPdfiumWrapper.h"
#include "debug.h"

namespace Targoman {
namespace PDFLA {
using namespace Targoman::DLA;
using namespace Targoman::Common;

constexpr float MAX_IMAGE_BLOB_AREA_FACTOR = 0.5f;
constexpr float MAX_WORD_SEPARATION_TO_MEAN_CHAR_WIDTH_RATIO = 2.f;

class clsPdfLaInternals {
 private:
  std::unique_ptr<clsPdfiumWrapper> PdfiumWrapper;

 private:
  float computeWordSeparationThreshold(
      const DocItemPtrVector_t &_sortedDocItems, float _meanCharWidth,
      float _width);
  BoundingBoxPtrVector_t getRawWhitespaceCover(
      const BoundingBoxPtr_t &_bounds, const BoundingBoxPtrVector_t &_obstacles,
      float _minCoverLegSize);
  BoundingBoxPtrVector_t getWhitespaceCoverage(
      const DocItemPtrVector_t &_sortedDocItems, const stuSize &_pageSize,
      float _meanCharWidth, float _wordSeparationThreshold);
  auto preparePreliminaryData(const DocItemPtrVector_t &_docItems,
                              const stuSize &_pageSize);
  std::tuple<DocLinePtrVector_t, DocItemPtrVector_t> findPageLinesAndFigures(
      const DocItemPtrVector_t &_sortedChars,
      const DocItemPtrVector_t &_sortedFigures,
      const BoundingBoxPtrVector_t &_whitespaceCover, const stuSize &_pageSize);
  DocBlockPtrVector_t findPageTextBlocks(
      const DocLinePtrVector_t &_pageLines,
      const DocItemPtrVector_t &_pageFigures,
      const BoundingBoxPtrVector_t &_whitespaceCover);

 public:
  clsPdfLaInternals(uint8_t *_data, size_t _size)
      : PdfiumWrapper(new clsPdfiumWrapper(_data, _size)) {}

  size_t pageCount();

 public:
  stuSize getPageSize(size_t _pageIndex);
  std::vector<uint8_t> renderPageImage(size_t _pageIndex,
                                       uint32_t _backgroundColor,
                                       const stuSize &_renderSize);

 public:
  DocBlockPtrVector_t getPageBlocks(size_t _pageIndex);
  DocBlockPtrVector_t getTextBlocks(size_t _pageIndex);
};

clsPdfLa::clsPdfLa(uint8_t *_data, size_t _size)
    : Internals(new clsPdfLaInternals(_data, _size)) {}

clsPdfLa::~clsPdfLa() { clsPdfLaDebug::instance().unregisterObject(this); }

size_t clsPdfLa::pageCount() { return this->Internals->pageCount(); }

Targoman::DLA::stuSize clsPdfLa::getPageSize(size_t _pageIndex) {
  return this->Internals->getPageSize(_pageIndex);
}

std::vector<uint8_t> clsPdfLa::renderPageImage(size_t _pageIndex,
                                               uint32_t _backgroundColor,
                                               const stuSize &_renderSize) {
  return this->Internals->renderPageImage(_pageIndex, _backgroundColor,
                                          _renderSize);
}

Targoman::DLA::DocBlockPtrVector_t clsPdfLa::getPageBlocks(size_t _pageIndex) {
  return this->Internals->getPageBlocks(_pageIndex);
}

DocBlockPtrVector_t clsPdfLa::getTextBlocks(size_t _pageIndex) {
  return this->Internals->getTextBlocks(_pageIndex);
}

void clsPdfLa::enableDebugging(const std::string &_basename) {
  if (!_basename.empty())
    clsPdfLaDebug::instance().registerObject(this->Internals.get(), _basename);
}

float clsPdfLaInternals::computeWordSeparationThreshold(
    const DocItemPtrVector_t &_sortedDocItems, float _meanCharWidth,
    float _width) {
  constexpr float MIN_ACKNOWLEDGABLE_DISTANCE = 3.f;
  constexpr float WORD_SEPARATION_THRESHOLD_MULTIPLIER = 1.5f;

  std::vector<int32_t> HorzDistanceHistogram(
      static_cast<size_t>(std::ceil(_width)), 0);
  for (size_t i = 1; i < _sortedDocItems.size(); ++i) {
    const auto &ThisItem = _sortedDocItems.at(i);
    const auto &PrevItem = _sortedDocItems.at(i - 1);
    if (ThisItem->BoundingBox.verticalOverlap(PrevItem->BoundingBox) >
        MIN_ITEM_SIZE) {
      auto dx = static_cast<int32_t>(ThisItem->BoundingBox.left() -
                                     PrevItem->BoundingBox.right() + 0.5f);
      if (dx >= MIN_ACKNOWLEDGABLE_DISTANCE &&
          dx <= MAX_WORD_SEPARATION_TO_MEAN_CHAR_WIDTH_RATIO * _meanCharWidth) {
        ++HorzDistanceHistogram[dx];
        if (dx > 1) ++HorzDistanceHistogram[dx - 1];
        if (dx < static_cast<int32_t>(HorzDistanceHistogram.size()) - 1)
          ++HorzDistanceHistogram[dx + 1];
      }
    }
  }

  return WORD_SEPARATION_THRESHOLD_MULTIPLIER *
         argmax(HorzDistanceHistogram, [](int32_t a) { return a; });
}

BoundingBoxPtrVector_t clsPdfLaInternals::getRawWhitespaceCover(
    const BoundingBoxPtr_t &_bounds, const BoundingBoxPtrVector_t &_obstacles,
    float _minCoverLegSize) {
  constexpr float MIN_COVER_SIZE = 4.f;
  constexpr float MIN_COVER_PERIMETER = 128.f;
  constexpr float MIN_COVER_AREA = 2048.f;
  constexpr size_t MAX_COVER_NUMBER_OF_ITEMS = 30;

  auto candidateIsAcceptable = [&](const BoundingBoxPtr_t &_bounds) {
    return _bounds->width() >= MIN_COVER_SIZE &&
           _bounds->height() >= MIN_COVER_SIZE &&
           _bounds->height() >= _bounds->width() &&
           _bounds->width() + _bounds->height() >= MIN_COVER_PERIMETER &&
           _bounds->area() >= MIN_COVER_AREA;
  };

  BoundingBoxPtrVector_t Result;
  if (true) {
    BoundingBoxPtrVector_t L2R, T2B;

    std::vector<float> Xs, Ys;
    Xs.push_back(_bounds->right());
    Ys.push_back(_bounds->bottom());
    for (const auto &Obstacle : _obstacles) {
      Xs.insert(Xs.end(), {Obstacle->left(), Obstacle->right()});
      Ys.insert(Ys.end(), {Obstacle->top(), Obstacle->bottom()});
    }
    std::sort(Xs.begin(), Xs.end());
    std::sort(Ys.begin(), Ys.end());

    auto filterPositions = [](std::vector<float> &_positions) {
      std::vector<float> Filtered;
      for (size_t i = 1; i < _positions.size(); ++i) {
        if (std::abs(_positions[i] - _positions[i - 1]) > MIN_COVER_SIZE) {
          if (Filtered.empty() || Filtered.back() != _positions[i - 1])
            Filtered.push_back(_positions[i - 1]);
          Filtered.push_back(_positions[i]);
        }
      }
      _positions = std::move(Filtered);
    };

    filterPositions(Xs);
    filterPositions(Ys);

    if (std::abs(Xs.front() - _bounds->left()) < MIN_COVER_SIZE)
      Xs.front() = _bounds->left();
    else
      Xs.insert(Xs.begin(), _bounds->left());
    if (std::abs(Ys.front() - _bounds->top()) < MIN_COVER_SIZE)
      Ys.front() = _bounds->top();
    else
      Ys.insert(Ys.begin(), _bounds->top());

    size_t NX = Xs.size(), NY = Ys.size();

    L2R.resize(NX * NY);
    T2B.resize(NX * NY);

    for (size_t iY = 1; iY < Ys.size(); ++iY) {
      for (size_t iX = 1; iX < Xs.size(); ++iX) {
        auto NewItem = std::make_shared<stuBoundingBox>(
            Xs[iX - 1] + 1, Ys[iY - 1] + 1, Xs[iX] - 1, Ys[iY] - 1);
        for (const auto &Obstacle : _obstacles)
          if (NewItem.get() != nullptr &&
              NewItem->hasIntersectionWith(Obstacle)) {
            NewItem.reset();
            break;
          }
        if (NewItem.get() != nullptr) {
          L2R[(iX - 1) + (iY - 1) * NX] = NewItem;
          T2B[(iY - 1) + (iX - 1) * NY] = NewItem;
        }
      }
    }

    for (size_t iX = 1; iX < NX; ++iX) {
      size_t Offset = (iX - 1) * NY;
      size_t i = 0;
      for (size_t j = 1; j < NY; ++j) {
        if (T2B[Offset + i].get() == nullptr) {
          i = j;
          continue;
        }
        if (T2B[Offset + j].get() == nullptr) {
          i = j;
          continue;
        }
        T2B[Offset + i]->unionWith_(T2B[Offset + j]);
        T2B[Offset + j].reset();
        L2R[(iX - 1) + j * NX].reset();
      }
    }

    for (size_t iY = 1; iY < NY; ++iY) {
      size_t Offset = (iY - 1) * NX;
      size_t i = 0;
      for (size_t j = 1; j < NX; ++j) {
        if (L2R[Offset + i].get() == nullptr) {
          i = j;
          continue;
        }
        if (L2R[Offset + j].get() == nullptr) {
          i = j;
          continue;
        }
        if (std::abs(L2R[Offset + i]->height() - L2R[Offset + j]->height()) >
                MIN_ITEM_SIZE ||
            L2R[Offset + i]->verticalOverlap(L2R[Offset + j]) <
                L2R[Offset + i]->height() - 1) {
          i = j;
          continue;
        }
        L2R[Offset + i]->unionWith_(L2R[Offset + j]);
        L2R[Offset + j].reset();
      }
    }

    Result = std::move(L2R);
    filterNullPtrs_(Result);
    filter_(Result, candidateIsAcceptable);
  }

  return Result;
}

BoundingBoxPtrVector_t clsPdfLaInternals::getWhitespaceCoverage(
    const DocItemPtrVector_t &_sortedDocItems, const stuSize &_pageSize,
    float _meanCharWidth, float _wordSeparationThreshold) {
  constexpr float APPROXIMATE_FULL_OVERLAP_RATIO = 0.95f;

  DocItemPtrVector_t Blobs;
  DocItemPtr_t PrevItem = nullptr;
  for (size_t i = 0; i < _sortedDocItems.size(); ++i) {
    const auto &ThisItem = _sortedDocItems.at(i);
    if (ThisItem->Type != enuDocItemType::Char) continue;
    if (Blobs.empty()) {
      Blobs.push_back(std::make_shared<stuDocItem>(*ThisItem));
      PrevItem = ThisItem;
      continue;
    }
    bool DoNotPushback = false;
    if (ThisItem->Type == PrevItem->Type &&
        ThisItem->BoundingBox.verticalOverlapRatio(PrevItem->BoundingBox) >
            0.5) {
      auto dx = static_cast<int32_t>(ThisItem->BoundingBox.left() -
                                     PrevItem->BoundingBox.right() + 0.5);
      if (dx < _wordSeparationThreshold) {
        Blobs.back()->BoundingBox.unionWith_(ThisItem->BoundingBox);
        DoNotPushback = true;
      }
    }
    if (!DoNotPushback)
      Blobs.push_back(std::make_shared<stuDocItem>(*ThisItem));
    PrevItem = ThisItem;
  }

  for (const auto &Item :
       filter(_sortedDocItems, [](const DocItemPtr_t &_item) {
         return _item->Type != enuDocItemType::Char;
       })) {
    if (Item->BoundingBox.area() <=
        MAX_IMAGE_BLOB_AREA_FACTOR * _pageSize.area())
      Blobs.push_back(Item);
  }

  auto RawCover = this->getRawWhitespaceCover(
      std::make_shared<stuBoundingBox>(stuPoint(), _pageSize),
      map(Blobs,
          [](const DocItemPtr_t &Item) {
            return std::make_shared<stuBoundingBox>(Item->BoundingBox);
          }),
      _meanCharWidth);

  auto Cover = filter(RawCover, [](const BoundingBoxPtr_t &a) {
    return a->width() < a->height();
  });
  RawCover = filter(RawCover, [](const BoundingBoxPtr_t &a) {
    return a->width() >= a->height();
  });
  for (auto &CoverItem : Cover) {
    for (const auto &HelperItem : RawCover)
      if (CoverItem->horizontalOverlap(HelperItem) >=
          APPROXIMATE_FULL_OVERLAP_RATIO * CoverItem->width()) {
        if (CoverItem->verticalOverlap(HelperItem) > -MIN_ITEM_SIZE) {
          float Y0 = std::min(CoverItem->top(), HelperItem->top());
          float Y1 = std::max(CoverItem->bottom(), HelperItem->bottom());
          CoverItem->Origin.Y = Y0;
          CoverItem->Size.Height = Y1 - Y0;
        }
      }
  }
  RawCover = Cover;
  Cover.clear();
  for (const auto &CandidateCoverItem : RawCover) {
    bool MergedWithAnotherItem = false;
    for (auto &StoredCoverItem : Cover)
      if (StoredCoverItem->hasIntersectionWith(CandidateCoverItem) &&
          StoredCoverItem->horizontalOverlapRatio(CandidateCoverItem) >=
              APPROXIMATE_FULL_OVERLAP_RATIO) {
        StoredCoverItem->unionWith_(CandidateCoverItem);
        MergedWithAnotherItem = true;
      }
    if (!MergedWithAnotherItem) Cover.push_back(CandidateCoverItem);
  }

  return Cover;
}

template <typename Item1_t, typename Item2_t>
bool areHorizontallyOnSameLine(const Item1_t &_item1, const Item2_t &_item2) {
  float HorizontalOverlap =
      _item2->BoundingBox.horizontalOverlap(_item1->BoundingBox);
  if (HorizontalOverlap <= -2.5f * std::max(_item1->BoundingBox.height(),
                                            _item2->BoundingBox.height()))
    return false;
  return true;
}

template <typename Item1_t, typename Item2_t>
bool areVerticallyOnSameLine(const Item1_t &_item1, const Item2_t &_item2) {
  float VerticalOverlap =
      _item2->BoundingBox.verticalOverlap(_item1->BoundingBox);
  if ((_item1->BoundingBox.height() < 0.5f * _item2->BoundingBox.height()) ||
      (_item2->BoundingBox.height() < 0.5f * _item1->BoundingBox.height())) {
    return VerticalOverlap > MIN_ITEM_SIZE;
  } else {
    return VerticalOverlap > 0.5f * std::min(_item1->BoundingBox.height(),
                                             _item2->BoundingBox.height());
  }
}

auto clsPdfLaInternals::preparePreliminaryData(
    const DocItemPtrVector_t &_docItems, const stuSize &_pageSize) {
  auto [SortedFigures, SortedChars] =
      std::move(split(_docItems, [&](const DocItemPtr_t &e) {
        return e->Type != enuDocItemType::Char;
      }));

  SortedChars = filter(SortedChars, [](const DocItemPtr_t &e) {
    return std::abs(e->BaselineAngle) < 0.01f * M_PIf;
  });

  sortByBoundingBoxes<enuBoundingBoxOrdering::T2BL2R>(SortedFigures);
  sortByBoundingBoxes<enuBoundingBoxOrdering::T2BL2R>(SortedChars);

  auto MeanCharWidth = mean(SortedChars, [](const DocItemPtr_t &e) {
    return e->BoundingBox.width();
  });
  auto WordSeparationThreshold = this->computeWordSeparationThreshold(
      SortedChars, MeanCharWidth, _pageSize.Width);
  auto WhitespaceCover =
      this->getWhitespaceCoverage(cat(SortedChars, SortedFigures), _pageSize,
                                  MeanCharWidth, WordSeparationThreshold);
  return std::make_tuple(SortedChars, SortedFigures, MeanCharWidth,
                         WordSeparationThreshold, WhitespaceCover);
}

std::tuple<DocLinePtrVector_t, DocItemPtrVector_t>
clsPdfLaInternals::findPageLinesAndFigures(
    const DocItemPtrVector_t &_sortedChars,
    const DocItemPtrVector_t &_sortedFigures,
    const BoundingBoxPtrVector_t &_whitespaceCover, const stuSize &_pageSize) {
  DocItemPtrVector_t ResultFigures;
  for (const auto &Item : _sortedFigures) {
    if (Item->BoundingBox.area() <=
        MAX_IMAGE_BLOB_AREA_FACTOR * _pageSize.area()) {
      DocItemPtr_t SameFigure{nullptr};
      for (auto &ResultFigureItem : ResultFigures)
        if (ResultFigureItem->BoundingBox.hasIntersectionWith(
                Item->BoundingBox)) {
          SameFigure = ResultFigureItem;
          break;
        }
      if (SameFigure.get() != nullptr)
        SameFigure->BoundingBox.unionWith_(Item->BoundingBox);
      else
        ResultFigures.push_back(Item);
    }
  }

  std::map<DocItemPtr_t, DocItemPtr_t> RightNeighbourOf;
  std::map<DocItemPtr_t, DocItemPtr_t> LeftNeighbourOf;
  for (const auto &Item : _sortedChars) {
    auto &RightNeighbour = RightNeighbourOf[Item] = DocItemPtr_t();
    for (const auto &OtherItem : _sortedChars) {
      if (OtherItem.get() == Item.get()) continue;
      size_t Direction = 0;
      float VerticalOverlap =
          Item->BoundingBox.verticalOverlap(OtherItem->BoundingBox);
      if (VerticalOverlap <= MIN_ITEM_SIZE) continue;

      float HorizontalOverlap =
          Item->BoundingBox.horizontalOverlap(OtherItem->BoundingBox);
      if (HorizontalOverlap < -2.f * std::max(Item->BoundingBox.height(),
                                              OtherItem->BoundingBox.height()))
        continue;

      if (VerticalOverlap <= HorizontalOverlap ||
          Item->BoundingBox.centerX() >= OtherItem->BoundingBox.centerX())
        continue;
      if (RightNeighbour.get() == nullptr) {
        RightNeighbour = OtherItem;
        continue;
      }

      float RightNeighbourHorizontalOverlap =
          RightNeighbour->BoundingBox.horizontalOverlap(Item->BoundingBox);
      float RightNeighbourVerticalOverlap =
          RightNeighbour->BoundingBox.verticalOverlap(Item->BoundingBox);

      constexpr float HORZ_OVERLAP_THRESHOLD = -1.f;

      if (HorizontalOverlap >= HORZ_OVERLAP_THRESHOLD) {
        if (RightNeighbourHorizontalOverlap >= HORZ_OVERLAP_THRESHOLD) {
          if (VerticalOverlap > RightNeighbourVerticalOverlap)
            RightNeighbour = OtherItem;
        } else
          RightNeighbour = OtherItem;
      } else {
        if (HorizontalOverlap > RightNeighbourHorizontalOverlap)
          RightNeighbour = OtherItem;
      }
    }
    LeftNeighbourOf[RightNeighbour] = Item;
  }

  std::set<DocItemPtr_t> UsedChars;
  auto getFirstUnusedChar = [&, &SortedChars = _sortedChars]() {
    for (auto &Char : SortedChars) {
      if (UsedChars.find(Char) == UsedChars.end()) return Char;
    }
    return DocItemPtr_t();
  };

  DocLinePtrVector_t ResultLines;
  do {
    auto Item = getFirstUnusedChar();
    while (LeftNeighbourOf[Item].get() != nullptr) {
      Item = LeftNeighbourOf[Item];
      if (UsedChars.find(Item) != UsedChars.end()) {
        std::cout << "THIS MUST NEVER HAPPEN" << std::endl;
      }
    }

    auto Line = std::make_shared<stuDocLine>();
    Line->BoundingBox = Item->BoundingBox;
    Line->Items.push_back(Item);
    UsedChars.insert(Item);

    // clsPdfLaDebug::instance().createImage(this).add(ResultLines).add(Line).show("NewLine");
    ResultLines.push_back(Line);

    while (RightNeighbourOf[Item].get() != nullptr) {
      Item = RightNeighbourOf[Item];

      auto Union = Line->BoundingBox.unionWith(Item->BoundingBox);
      bool OverlapsWithCover = false;

      // TODO: Deal with this magic constant
      for (const auto &CoverItem : _whitespaceCover) {
        auto Intersection = CoverItem->intersectWith(Union);
        if (Intersection.width() > 1.f &&
            Intersection.height() > Union.height() - MIN_ITEM_SIZE) {
          OverlapsWithCover = true;
          // clsPdfLaDebug::instance()
          //     .createImage(this)
          //     .add(Line)
          //     .add(Item)
          //     .add(CoverItem)
          //     .show("WhiteCoverOverlap");
          break;
        }
      }
      if (OverlapsWithCover) {
        Line = std::make_shared<stuDocLine>();
        Line->BoundingBox = Item->BoundingBox;
        // clsPdfLaDebug::instance().createImage(this).add(ResultLines).add(Line).show("NewLine");
        ResultLines.push_back(Line);
      } else {
        // auto WholeBox = Line->BoundingBox.unionWith(Item->BoundingBox);
        // auto Problems =
        //     filter(_whitespaceCover, [&](const BoundingBoxPtr_t &e) {
        //       return WholeBox.hasIntersectionWith(e);
        //     });
        // if (Problems.size())
        //   clsPdfLaDebug::instance()
        //       .createImage(this)
        //       .add(Line)
        //       .add(Item)
        //       .add(Problems)
        //       .save("UpdateLine")
        //       .show("UpdateLine");
        Line->BoundingBox.unionWith_(Item->BoundingBox);
        Line->Items.push_back(Item);
      }
      Line->Items.push_back(Item);

      UsedChars.insert(Item);
    }

  } while (UsedChars.size() < _sortedChars.size());

  // clsPdfLaDebug::instance()
  //     .createImage(this)
  //     .add(ResultLines)
  //     .save("LineSegments")
  //     .show("LineSegments");

  for (const auto &LineSegment : ResultLines) {
    if (LineSegment.get() == nullptr) continue;
    std::vector<size_t> IndexesOfSegmentsOfSameLine;
    for (size_t i = 0; i < ResultLines.size(); ++i) {
      auto &l = ResultLines[i];
      if (l.get() == nullptr) continue;
      if (l.get() == LineSegment.get()) {
        IndexesOfSegmentsOfSameLine.push_back(i);
        continue;
      }
      if (!areVerticallyOnSameLine(LineSegment, l)) continue;
      auto Union = LineSegment->BoundingBox.unionWith(l->BoundingBox);
      bool OverlapsWithCover = false;
      for (const auto &CoverItem : _whitespaceCover)
        if (CoverItem->hasIntersectionWith(Union) &&
            CoverItem->verticalOverlap(Union) > 3) {
          OverlapsWithCover = true;
          break;
        }
      if (OverlapsWithCover) continue;
      IndexesOfSegmentsOfSameLine.push_back(i);
    };
    if (IndexesOfSegmentsOfSameLine.size() < 2) continue;
    std::sort(IndexesOfSegmentsOfSameLine.begin(),
              IndexesOfSegmentsOfSameLine.end(), [&](size_t a, size_t b) {
                return ResultLines[a]->BoundingBox.left() <
                       ResultLines[b]->BoundingBox.left();
              });
    // clsPdfLaDebug::instance()
    //     .createImage(this)
    //     .add(map(IndexesOfSegmentsOfSameLine,
    //              [&](size_t i) { return ResultLines[i]; }))
    //     .show("MergeLineSegments");
    size_t ProxySegmentIndex = static_cast<size_t>(-1);
    DocLinePtr_t Merged;
    for (size_t i : IndexesOfSegmentsOfSameLine) {
      if (Merged.get() == nullptr) {
        Merged = ResultLines[i];
        ProxySegmentIndex = i;
      } else {
        if (Merged->BoundingBox.horizontalOverlap(ResultLines[i]->BoundingBox) <
            -std::max(Merged->BoundingBox.height(),
                      ResultLines[i]->BoundingBox.height()))
          break;
        Merged->mergeWith_(ResultLines[i]);
      }
      ResultLines[i].reset();
    }
    ResultLines[ProxySegmentIndex].swap(Merged);
  }

  filterNullPtrs_(ResultLines);

  // clsPdfLaDebug::instance()
  //     .createImage(this)
  //     .add(ResultLines)
  //     .save("Lines")
  //     .show("Lines");

  return std::make_tuple(ResultLines, ResultFigures);
}

DocBlockPtrVector_t clsPdfLaInternals::findPageTextBlocks(
    const DocLinePtrVector_t &_pageLines,
    const DocItemPtrVector_t &_pageFigures,
    const BoundingBoxPtrVector_t &_whitespaceCover) {
  auto SortedLines = _pageLines;
  sortByBoundingBoxes<enuBoundingBoxOrdering::T2BL2R>(SortedLines);

  std::map<DocLinePtr_t, DocLinePtr_t> TopNeighbours;
  std::map<DocLinePtr_t, DocLinePtr_t> BottomNeighbours;
  for (auto &Line : SortedLines) {
    Line->computeBaseline();
    auto &BottomNeighbour = BottomNeighbours[Line];
    float BottomNeighbourVerticalOverlap = std::numeric_limits<float>::min();
    float BottomNeighbourHorizontalOverlap = std::numeric_limits<float>::min();
    for (auto &OtherLine : SortedLines) {
      if (Line.get() == OtherLine.get()) continue;
      if (Line->BoundingBox.top() >= OtherLine->BoundingBox.top() ||
          Line->BoundingBox.bottom() >= OtherLine->BoundingBox.bottom())
        continue;
      float HorizontalOverlap =
          Line->BoundingBox.horizontalOverlap(OtherLine->BoundingBox);
      // TODO: Deal with this magic constant
      if (HorizontalOverlap < -5.f) continue;
      float VerticalOverlap =
          Line->BoundingBox.verticalOverlap(OtherLine->BoundingBox);
      // TODO: Deal with this magic constant
      if (VerticalOverlap < -3.f * Line->BoundingBox.height()) continue;
      if (BottomNeighbour.get() == nullptr) {
        BottomNeighbour = OtherLine;
        continue;
      }
      if (VerticalOverlap > BottomNeighbourVerticalOverlap) {
        if (BottomNeighbourVerticalOverlap < -MIN_ITEM_SIZE)
          BottomNeighbour = OtherLine;
        else if (HorizontalOverlap > BottomNeighbourHorizontalOverlap)
          BottomNeighbour = OtherLine;
      }
    }
    TopNeighbours[BottomNeighbour] = Line;
  }

  std::set<DocLinePtr_t> UsedLines;
  auto getFirstUnusedLine = [&]() {
    for (auto &Line : SortedLines) {
      if (UsedLines.find(Line) == UsedLines.end()) return Line;
    }
    return DocLinePtr_t();
  };

  DocBlockPtrVector_t Result;
  do {
    auto Line = getFirstUnusedLine();
    if (UsedLines.find(Line) != UsedLines.end()) {
      std::cout << "THIS MUST NEVER HAPPEN" << std::endl;
    }

    bool Consumed = false;
    for (auto &Block : Result) {
      if (Block->BoundingBox.contains(Line->BoundingBox)) {
        // auto Problems =
        //     filter(_whitespaceCover, [&](const BoundingBoxPtr_t &b) {
        //       return b->hasIntersectionWith(
        //           Block->BoundingBox.unionWith(Line->BoundingBox));
        //     });
        // if (!Problems.empty())
        //   clsPdfLaDebug::instance()
        //       .createImage(this)
        //       .add(Block)
        //       .add(Line)
        //       .add(Problems)
        //       .show("Consume");

        Block->BoundingBox.unionWith_(Line->BoundingBox);
        Block.asText()->Lines.push_back(Line);
        UsedLines.insert(Line);
        Consumed = true;
        break;
      }
    }
    if (Consumed) continue;

    for (const auto &OtherLine :
         sortedByBoundingBoxes<enuBoundingBoxOrdering::L2R>(
             filter(SortedLines, [&Line](const DocLinePtr_t &_otherLine) {
               return _otherLine->BoundingBox.verticalOverlap(
                          Line->BoundingBox) > MIN_ITEM_SIZE;
             }))) {
      if (UsedLines.find(OtherLine) == UsedLines.end()) {
        Line = OtherLine;
        break;
      }
    }

    clsDocBlockPtr Block;
    Block.reset(new stuDocTextBlock);
    Block->BoundingBox = Line->BoundingBox;
    Result.push_back(Block);

    Block.asText()->Lines.push_back(Line);
    UsedLines.insert(Line);

    while (BottomNeighbours[Line].get() != nullptr) {
      auto PrevLine = Line;
      Line = BottomNeighbours[Line];
      auto NextLine = BottomNeighbours[Line];
      if (NextLine.get() != nullptr) {
        float V0 = PrevLine->BoundingBox.verticalOverlap(Line->BoundingBox);
        float V1 = NextLine->BoundingBox.verticalOverlap(Line->BoundingBox);
        if (V0 < V1 - 2.f && V0 < -std::min({PrevLine->BoundingBox.height(),
                                             Line->BoundingBox.height(),
                                             NextLine->BoundingBox.height()}))
          break;
      }
      auto Union = Block->BoundingBox.unionWith(Line->BoundingBox);
      bool Blocked = false;
      for (const auto &PossibleBlocker : _pageFigures)
        if (Union.hasIntersectionWith(PossibleBlocker->BoundingBox)) {
          Blocked = true;
          break;
        }
      if (Blocked) break;
      auto LinesOnSameVerticalStripe =
          filter(SortedLines, [&](const DocLinePtr_t &l) {
            auto c = [](const stuBoundingBox &a, const stuBoundingBox &b,
                        const stuBoundingBox &c) {
              return a.horizontalOverlap(b) > MIN_ITEM_SIZE &&
                     a.verticalOverlap(c) > MIN_ITEM_SIZE;
            };
            return c(l->BoundingBox, Union, Line->BoundingBox) ||
                   c(l->BoundingBox, Line->BoundingBox, Union);
          });
      // TODO: Deal with this magic constant
      for (const auto &PossibleBlocker : _whitespaceCover)
        if (PossibleBlocker->left() >
                Union.left() - 2.f &&  // The blocker is not attached left
            PossibleBlocker->right() <
                Union.right() + 2.f &&  // The blocker is not attached right
            Union.hasIntersectionWith(PossibleBlocker) &&
            any(LinesOnSameVerticalStripe,
                [&](const DocLinePtr_t &_line) {
                  return _line->BoundingBox.left() <= PossibleBlocker->left();
                }) &&
            any(LinesOnSameVerticalStripe, [&](const DocLinePtr_t &_line) {
              return _line->BoundingBox.right() >= PossibleBlocker->right();
            })) {
          Blocked = true;
          break;
        }
      if (Blocked) break;

      // auto Problems = filter(_whitespaceCover, [&](const BoundingBoxPtr_t &b) {
      //   return b->hasIntersectionWith(
      //       Block->BoundingBox.unionWith(Line->BoundingBox));
      // });
      // if (!Problems.empty())
      //   clsPdfLaDebug::instance()
      //       .createImage(this)
      //       .add(Block)
      //       .add(Line)
      //       .add(LinesOnSameVerticalStripe)
      //       .add(Problems)
      //       .show("AddToBlock");

      Block->BoundingBox.unionWith_(Line->BoundingBox);
      Block.asText()->Lines.push_back(Line);
      UsedLines.insert(Line);
    }

  } while (UsedLines.size() < SortedLines.size());

  for (auto &Item : Result) {
    if (Item->Type != enuDocBlockType::Text) continue;
    for (auto &OtherItem : Result) {
      if (OtherItem.get() == nullptr) continue;
      if (Item.get() == OtherItem.get()) continue;
      if (Item->Type != OtherItem->Type) continue;
      if (OtherItem->BoundingBox.contains(Item->BoundingBox)) {
        OtherItem.asText()->mergeWith_(Item);
        Item.reset();
        break;
      }
    }
  }

  filterNullPtrs_(Result);

  return Result;
}

size_t clsPdfLaInternals::pageCount() {
  return this->PdfiumWrapper->pageCount();
}

stuSize clsPdfLaInternals::getPageSize(size_t _pageIndex) {
  return this->PdfiumWrapper->getPageSize(_pageIndex);
}

std::vector<uint8_t> clsPdfLaInternals::renderPageImage(
    size_t _pageIndex, uint32_t _backgroundColor, const stuSize &_renderSize) {
  if (this->PdfiumWrapper.get() == nullptr) return std::vector<uint8_t>();

  const auto &[PrepopulatedData, PrepopulatedDataSize] =
      clsPdfLaDebug::instance().getPageImage(this, _pageIndex);
  if (PrepopulatedData.size() > 0 && PrepopulatedDataSize == _renderSize)
    return PrepopulatedData;

  auto Data = this->PdfiumWrapper->renderPageImage(_pageIndex, _backgroundColor,
                                                   _renderSize);

  return Data;
}

DocBlockPtrVector_t clsPdfLaInternals::getPageBlocks(size_t _pageIndex) {
  auto PageSize = this->getPageSize(_pageIndex);

  if (clsPdfLaDebug::instance().isObjectRegistered(this)) {
    clsPdfLaDebug::instance().setCurrentPageIndex(this, _pageIndex);
    auto [PageImage, PageImageSize] =
        clsPdfLaDebug::instance().getPageImage(this, _pageIndex);

    if (PageImage.size() == 0 ||
        PageImageSize != PageSize.scale(DEBUG_UPSCALE_FACTOR)) {
      auto Data = this->renderPageImage(_pageIndex, 0xffffffff,
                                        PageSize.scale(DEBUG_UPSCALE_FACTOR));
      clsPdfLaDebug::instance().registerPageImage(
          this, _pageIndex, Data, PageSize.scale(DEBUG_UPSCALE_FACTOR));
    }
  }

  auto Items = filter(this->PdfiumWrapper->getPageItems(_pageIndex),
                      [](const DocItemPtr_t &e) {
                        return e->BoundingBox.width() > MIN_ITEM_SIZE &&
                               e->BoundingBox.height() > MIN_ITEM_SIZE;
                      });
  auto [SortedChars, SortedFigures, MeanCharWidth, WordSeparationThreshold,
        WhitespaceCover] =
      std::move(this->preparePreliminaryData(Items, PageSize));
  auto [Lines, Figures] = std::move(this->findPageLinesAndFigures(
      SortedChars, SortedFigures, WhitespaceCover, PageSize));
  auto Blocks = this->findPageTextBlocks(Lines, Figures, WhitespaceCover);

  // clsPdfLaDebug::instance()
  //     .createImage(this)
  //     .add(Blocks)
  //     .add(Figures)
  //     .add(WhitespaceCover)
  //     .show("DDD");

  for (const auto Item : Figures) {
    clsDocBlockPtr FigureBlock;
    FigureBlock.reset(new stuDocFigureBlock);
    FigureBlock->BoundingBox = Item->BoundingBox;
    Blocks.push_back(FigureBlock);
  }
  return Blocks;
}

DocBlockPtrVector_t clsPdfLaInternals::getTextBlocks(size_t _pageIndex) {
  clsPdfLaDebug::instance().setCurrentPageIndex(this, _pageIndex);

  auto PageSize = this->getPageSize(_pageIndex);
  auto Items = this->PdfiumWrapper->getPageItems(_pageIndex);
  auto [SortedChars, SortedFigures, MeanCharWidth, WordSeparationThreshold,
        WhitespaceCover] =
      std::move(this->preparePreliminaryData(Items, PageSize));
  auto [Lines, Figures] = std::move(this->findPageLinesAndFigures(
      SortedChars, SortedFigures, WhitespaceCover, PageSize));
  auto Blocks = this->findPageTextBlocks(Lines, Figures, WhitespaceCover);
  return Blocks;
}

void setDebugOutputPath(const std::string &_path) {
  clsPdfLaDebug::instance().setDebugOutputPath(_path);
}

}  // namespace PDFLA
}  // namespace Targoman
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
constexpr float APPROXIMATE_FULL_OVERLAP_RATIO = 0.95f;

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
      const BoundingBoxPtrVector_t &_whitespaceCover, float _meanCharWidth,
      float _meanCharHeight, const stuSize &_pageSize);
  DocBlockPtrVector_t findTextBlocks(
      const stuBoundingBox &_pageBounds, const DocLinePtrVector_t &_pageLines,
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

  BoundingBoxPtrVector_t Result;
  auto candidateIsAcceptable = [&](const BoundingBoxPtr_t &_bounds) {
    return _bounds->width() >= std::max(_minCoverLegSize, MIN_COVER_SIZE) &&
           _bounds->height() >=
               3.f * std::max(_minCoverLegSize, MIN_COVER_SIZE) &&
           _bounds->width() + _bounds->height() >= MIN_COVER_PERIMETER &&
           _bounds->area() >= MIN_COVER_AREA;
  };
  auto calculateCandidateScore = [&](const BoundingBoxPtr_t &_candidate) {
    constexpr float WLT = 2.f;
    constexpr float WHT = 4.f;
    const float WidthThresholdLow = std::min(WHT, WLT * _minCoverLegSize);
    const float WidthThresholdHigh =
        std::min(WLT * WHT, WHT * _minCoverLegSize);
    if (_candidate->width() <= WidthThresholdLow)
      return _candidate->height() + _candidate->width();
    else if (_candidate->width() > WidthThresholdHigh)
      return 2.f * _candidate->height();
    else {
      float C = std::cos(M_PIf * (_candidate->width() - WidthThresholdLow) /
                         (WidthThresholdHigh - WidthThresholdLow));
      return (2.f / (1 + C)) * (_candidate->height() + C * _candidate->width());
    }
  };
  auto findNextLargetsCover =
      [&](const BoundingBoxPtr_t &_bounds,
          const BoundingBoxPtrVector_t &_obstacles) -> BoundingBoxPtr_t {
    typedef std::tuple<float, BoundingBoxPtr_t, BoundingBoxPtrVector_t>
        Candidate_t;
    std::vector<Candidate_t> Candidates{
        std::make_tuple(calculateCandidateScore(_bounds), _bounds, _obstacles)};
    while (true) {
      if (Candidates.empty()) return std::make_shared<stuBoundingBox>();

      auto ArgMax = argmax(Candidates, [&](const Candidate_t &_candidate) {
        return candidateIsAcceptable(std::get<1>(_candidate))
                   ? std::get<0>(_candidate)
                   : -1;
      });

      auto [Score, Cover, Obstacles] = std::move(Candidates[ArgMax]);
      Candidates.erase(Candidates.begin() + ArgMax);

      if (Obstacles.empty() || Score < 1) {
        // clsPdfLaDebug::instance()
        //     .createImage(this)
        //     .add(_bounds)
        //     .add(Cover)
        //     .add(_obstacles)
        //     .show("Cover");
        return Cover;
      }
      auto Union = stuBoundingBox(_bounds->right(), _bounds->bottom(),
                                  _bounds->left(), _bounds->top());
      for (const auto &Obstacle : Obstacles) Union.unionWith_(Obstacle);
      auto Center = Union.center();
      auto Pivot =
          minElement(Obstacles, [&Center](const BoundingBoxPtr_t &_obstacle) {
            return -_obstacle->area();
          });

      BoundingBoxPtrVector_t NewCandidates{
          std::make_shared<stuBoundingBox>(Pivot->right() + MIN_ITEM_SIZE,
                                           Cover->top(), Cover->right(),
                                           Cover->bottom()),
          std::make_shared<stuBoundingBox>(Cover->left(), Cover->top(),
                                           Pivot->left() - MIN_ITEM_SIZE,
                                           Cover->bottom()),
          std::make_shared<stuBoundingBox>(Cover->left(),
                                           Pivot->bottom() + MIN_ITEM_SIZE,
                                           Cover->right(), Cover->bottom()),
          std::make_shared<stuBoundingBox>(Cover->left(), Cover->top(),
                                           Cover->right(),
                                           Pivot->top() - MIN_ITEM_SIZE)};

      // clsPdfLaDebug::instance()
      //     .createImage(this)
      //     .add(_bounds)
      //     .add(Cover)
      //     .add(Obstacles)
      //     .add(NewCandidates)
      //     .add(Pivot)
      //     .show("NewCandidates");

      for (auto &NewCandidate : NewCandidates) {
        if (!candidateIsAcceptable(NewCandidate)) continue;
        Candidates.push_back(std::make_tuple(
            calculateCandidateScore(NewCandidate), NewCandidate,
            filter(Obstacles, [&NewCandidate](const BoundingBoxPtr_t &Item) {
              return Item->hasIntersectionWith(NewCandidate);
            })));
      }
    }
  };
  auto Obstacles = _obstacles;
  for (size_t i = 0; i < MAX_COVER_NUMBER_OF_ITEMS; ++i) {
    auto NextCover = findNextLargetsCover(_bounds, Obstacles);
    if (!candidateIsAcceptable(NextCover)) break;
    Result.push_back(NextCover);
    Obstacles.push_back(NextCover);
  }

  return Result;
}

BoundingBoxPtrVector_t clsPdfLaInternals::getWhitespaceCoverage(
    const DocItemPtrVector_t &_sortedDocItems, const stuSize &_pageSize,
    float _meanCharHeight, float _wordSeparationThreshold) {
  BoundingBoxPtrVector_t Blobs;
  DocItemPtr_t PrevItem = nullptr;
  for (size_t i = 0; i < _sortedDocItems.size(); ++i) {
    const auto &ThisItem = _sortedDocItems.at(i);
    if (ThisItem->Type != enuDocItemType::Char) continue;
    if (Blobs.empty()) {
      Blobs.push_back(std::make_shared<stuBoundingBox>(ThisItem->BoundingBox));
      PrevItem = ThisItem;
      continue;
    }
    bool DoNotPushback = false;
    // TODO: Deal with this magic constant
    if (ThisItem->Type == PrevItem->Type &&
        ThisItem->BoundingBox.verticalOverlapRatio(PrevItem->BoundingBox) >
            0.5f) {
      auto dx = std::abs(static_cast<int32_t>(
          ThisItem->BoundingBox.left() - PrevItem->BoundingBox.right() + 0.5f));
      if (dx < std::max(_wordSeparationThreshold,
                        std::min(PrevItem->BoundingBox.height(),
                                 ThisItem->BoundingBox.height()))) {
        Blobs.back()->unionWith_(ThisItem->BoundingBox);
        DoNotPushback = true;
      }
    }
    if (!DoNotPushback) {
      Blobs.push_back(std::make_shared<stuBoundingBox>(ThisItem->BoundingBox));
    }
    PrevItem = ThisItem;
  }

  while (true) {
    bool ChangeOcurred = false;
    for (auto &Blob : Blobs) {
      if (Blob.get() == nullptr) continue;
      for (auto &OtherBlob : Blobs) {
        if (OtherBlob.get() == nullptr) continue;
        if (OtherBlob == Blob) continue;
        if (Blob->verticalOverlap(OtherBlob) > -_meanCharHeight &&
            Blob->horizontalOverlap(OtherBlob) > _meanCharHeight) {
          Blob->unionWith_(OtherBlob);
          OtherBlob.reset();
          ChangeOcurred = true;
        }
      }
    }
    if (!ChangeOcurred) break;
  }
  filterNullPtrs_(Blobs);

  auto Cover = this->getRawWhitespaceCover(
      std::make_shared<stuBoundingBox>(stuPoint(), _pageSize), Blobs,
      _meanCharHeight);

  if (!Cover.empty()) {
    sort_(Cover, [](const BoundingBoxPtr_t &e) { return e->height(); });
    for (auto Iterator = ++Cover.begin(); Iterator != Cover.end(); ++Iterator) {
      auto OtherIterator = Iterator;
      while (OtherIterator != Cover.begin()) {
        --OtherIterator;
        if (OtherIterator->get() == nullptr) continue;
        if (Iterator->get()->verticalOverlapRatio(*OtherIterator) >=
            APPROXIMATE_FULL_OVERLAP_RATIO) {
          auto HorizontalUnion =
              OtherIterator->get()->unionWithInDirection(*Iterator, true);
          if (!any(Blobs, [&](const BoundingBoxPtr_t &e) {
                return e->hasIntersectionWith(HorizontalUnion);
              }))
            OtherIterator->reset();
        }
      }
    }
  }
  filterNullPtrs_(Cover);

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
  auto MeanCharHeight = mean(SortedChars, [](const DocItemPtr_t &e) {
    return e->BoundingBox.height();
  });
  auto WordSeparationThreshold = this->computeWordSeparationThreshold(
      SortedChars, MeanCharWidth, _pageSize.Width);
  auto WhitespaceCover =
      this->getWhitespaceCoverage(cat(SortedChars, SortedFigures), _pageSize,
                                  MeanCharHeight, WordSeparationThreshold);

  return std::make_tuple(SortedChars, SortedFigures, MeanCharWidth,
                         MeanCharHeight, WordSeparationThreshold,
                         WhitespaceCover);
}

std::tuple<DocLinePtrVector_t, DocItemPtrVector_t>
clsPdfLaInternals::findPageLinesAndFigures(
    const DocItemPtrVector_t &_sortedChars,
    const DocItemPtrVector_t &_sortedFigures,
    const BoundingBoxPtrVector_t &_whitespaceCover, float _meanCharWidth,
    float _meanCharHeight, const stuSize &_pageSize) {
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

          break;
        }
      }
      if (OverlapsWithCover) {
        Line = std::make_shared<stuDocLine>();
        Line->BoundingBox = Item->BoundingBox;
        ResultLines.push_back(Line);
      } else {
        Line->BoundingBox.unionWith_(Item->BoundingBox);
        Line->Items.push_back(Item);
      }
      Line->Items.push_back(Item);

      UsedChars.insert(Item);
    }

  } while (UsedChars.size() < _sortedChars.size());

  for (auto &Figure : ResultFigures) {
    // TODO: Handle these magic constants
    if (Figure->BoundingBox.height() > 2.f * _meanCharHeight) continue;
    // if (Figure->BoundingBox.width() > 20.f * _meanCharWidth) continue;
    for (auto &Line : ResultLines) {
      if (Line.get() == nullptr) continue;
      // TODO: Handle these magic constants
      if (Figure->BoundingBox.height() < 1.5f * Line->BoundingBox.height()) {
        if (Figure->BoundingBox.horizontalOverlap(Line->BoundingBox) >
                -2.f * _meanCharHeight &&
            Figure->BoundingBox.verticalOverlap(Line->BoundingBox) > -4.f) {
          Line->BoundingBox.unionWith_(Figure->BoundingBox);
          Line->Items.push_back(Figure);
          Figure.reset();
          break;
        }
      }
    }
  }

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
  filterNullPtrs_(ResultFigures);

  // TODO: Make line elements sorted and text extractable

  return std::make_tuple(ResultLines, ResultFigures);
}

DocBlockPtrVector_t clsPdfLaInternals::findTextBlocks(
    const stuBoundingBox &_pageBounds, const DocLinePtrVector_t &_pageLines,
    const DocItemPtrVector_t &_pageFigures,
    const BoundingBoxPtrVector_t &_whitespaceCover) {
  auto SortedLines = _pageLines;
  sortByBoundingBoxes<enuBoundingBoxOrdering::T2BL2R>(SortedLines);

  std::map<DocLinePtr_t, DocLinePtr_t> TopNeighbours;
  std::map<DocLinePtr_t, DocLinePtr_t> BottomNeighbours;
  DocLinePtr_t PageNumberLine;
  for (auto &Line : SortedLines) {
    Line->computeBaseline();
    auto &BottomNeighbour = BottomNeighbours[Line];
    float BottomNeighbourVerticalOverlap = std::numeric_limits<float>::min();
    float BottomNeighbourHorizontalOverlap = std::numeric_limits<float>::min();
    bool HasNoLinesUnderneath = true;
    for (auto &OtherLine : SortedLines) {
      if (Line.get() == OtherLine.get()) continue;
      if (Line->BoundingBox.top() >= OtherLine->BoundingBox.top() ||
          Line->BoundingBox.bottom() >= OtherLine->BoundingBox.bottom())
        continue;
      // TODO: Deal with this magic constant
      if (OtherLine->BoundingBox.horizontalOverlap(Line->BoundingBox) > 1.f) {
        if (Line->BoundingBox.bottom() > 760) {
          std::cout << "CHECK THIS: " << OtherLine << std::endl;
          std::cout << "HasBoundingBoxMember:" << HasNoLinesUnderneath
                    << std::endl;
        }
        HasNoLinesUnderneath = false;
      }
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
    if (Line->BoundingBox.bottom() > 760) {
      std::cout << "HasNoLinesUnderneath:" << HasNoLinesUnderneath << std::endl;
    }
    if (HasNoLinesUnderneath) {
      if (Line->BoundingBox.left() < _pageBounds.centerX() &&
          Line->BoundingBox.right() > _pageBounds.centerX())
        PageNumberLine = Line;
    }
  }

  std::set<DocLinePtr_t> UsedLines;
  if (PageNumberLine.get() != nullptr) UsedLines.insert(PageNumberLine);
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
      if (UsedLines.find(Line) != UsedLines.end()) break;
      auto WidthThreshold = 4.f * std::min(Line->BoundingBox.height(),
                                           Block->BoundingBox.height());
      if (Block->BoundingBox.width() < WidthThreshold &&
          Line->BoundingBox.width() < WidthThreshold) {
        break;
      }
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
      Block->BoundingBox.unionWith_(Line->BoundingBox);
      Block.asText()->Lines.push_back(Line);
      UsedLines.insert(Line);
    }

  } while (UsedLines.size() < SortedLines.size());

  if (PageNumberLine.get() != nullptr) {
    clsDocBlockPtr Block;
    Block.reset(new stuDocTextBlock);
    Block->BoundingBox = PageNumberLine->BoundingBox;
    Result.push_back(Block);

    Block.asText()->Lines.push_back(PageNumberLine);
  }

  // Merge reference number into the text block
  for (auto &Item : Result) {
    if (Item.get() == nullptr) continue;
    if (Item->BoundingBox.width() < _pageBounds.width() / 5) continue;
    if (Item.asText()->Lines.size() < 2) continue;
    clsDocBlockPtr RefNumber;
    for (auto &OtherItem : Result) {
      if (OtherItem.get() == nullptr) continue;
      // TODO: Deal with this magic constant
      if (OtherItem->BoundingBox.width() > Item->BoundingBox.width() / 8)
        continue;
      if (OtherItem->BoundingBox.right() >= Item->BoundingBox.left()) continue;
      if (OtherItem->BoundingBox.verticalOverlapRatio(Item->BoundingBox) <
          APPROXIMATE_FULL_OVERLAP_RATIO)
        continue;
      if (RefNumber.get() == nullptr)
        RefNumber = OtherItem;
      else if (RefNumber->BoundingBox.horizontalOverlap(Item->BoundingBox) <
               OtherItem->BoundingBox.horizontalOverlap(Item->BoundingBox))
        RefNumber = OtherItem;
    }

    // TODO: Deal with this magic constant
    if (RefNumber &&
        RefNumber->BoundingBox.horizontalOverlap(Item->BoundingBox) >
            -5.f * std::min(
                       RefNumber.asText()->Lines.front()->BoundingBox.height(),
                       Item.asText()->Lines.front()->BoundingBox.height())) {
      auto Union = Item->BoundingBox.unionWith(RefNumber->BoundingBox);
      auto AllRefNumbers = filter(Result, [&](const clsDocBlockPtr &e) {
        return e.get() != nullptr && e.get() != Item.get() &&
               e->BoundingBox.hasIntersectionWith(Union);
      });

      // TODO: Deal with this magic constant
      if (all(AllRefNumbers, [&](const clsDocBlockPtr &e) {
            return e->BoundingBox.width() <= Item->BoundingBox.width() / 8 &&
                   e->BoundingBox.horizontalOverlapRatio(
                       RefNumber->BoundingBox) >=
                       APPROXIMATE_FULL_OVERLAP_RATIO;
          })) {
        for (auto &RefNumber : AllRefNumbers) {
          Item.asText()->mergeWith_(RefNumber);
          RefNumber.reset();
        }
      }
    }
  }

  auto contains = [&](const clsDocBlockPtr &a, const clsDocBlockPtr &b) {
    auto Intersection = a->BoundingBox.intersectWith(b->BoundingBox);
    return Intersection.area() > 0.75 * b->BoundingBox.area();
  };

  // Merge block that are completely inside another one into the bigger one
  for (auto &Item : Result) {
    if (Item.get() == nullptr) continue;
    if (Item->Type != enuDocBlockType::Text) continue;
    for (auto &OtherItem : Result) {
      if (OtherItem.get() == nullptr) continue;
      if (Item.get() == OtherItem.get()) continue;
      if (Item->Type != OtherItem->Type) continue;
      if (contains(OtherItem, Item)) {
        OtherItem.asText()->mergeWith_(Item);
        Item.reset();
        break;
      }
    }
  }

  // Resolve overlap between blocks
  if (true) {
    sort_(Result,
          [](const clsDocBlockPtr &b) { return -b->BoundingBox.area(); });
    auto Iterator = Result.begin();
    while (Iterator != Result.end()) {
      if (Iterator->get() == nullptr) {
        ++Iterator;
        continue;
      }
      auto OverlappingBlocks = filter(Result, [&](const clsDocBlockPtr &b) {
        return b.get() != Iterator->get() && b.get() != nullptr &&
               b->BoundingBox.hasIntersectionWith(Iterator->get()->BoundingBox);
      });
      if (OverlappingBlocks.empty()) continue;
      // Gather all lines in all these overlapping blocks
      DocLinePtrVector_t AllLines = Iterator->asText()->Lines;
      for (auto &Block : OverlappingBlocks)
        AllLines.insert(AllLines.end(), Block.asText()->Lines.begin(),
                        Block.asText()->Lines.end());
      sortByBoundingBoxes<enuBoundingBoxOrdering::T2BL2R>(AllLines);
      for (auto &Line : AllLines) {
        if (Line.get() == nullptr) continue;
        for (auto &OtherLine : AllLines) {
          if (Line.get() == OtherLine.get() || OtherLine.get() == nullptr)
            continue;
          if (OtherLine->BoundingBox.hasIntersectionWith(Line->BoundingBox)) {
            Line->mergeWith_(OtherLine);
            OtherLine.reset();
          }
        }
      }
      // Scatter lines in new properly shaped blocks
      std::vector<float> Y{Iterator->get()->BoundingBox.top(),
                           Iterator->get()->BoundingBox.bottom()};
      for (auto &Block : OverlappingBlocks)
        Y.insert(Y.end(),
                 {Block->BoundingBox.top(), Block->BoundingBox.bottom()});
      auto X0 = std::min(Iterator->get()->BoundingBox.left(),
                         min(OverlappingBlocks, [](const clsDocBlockPtr &b) {
                           return b->BoundingBox.left();
                         }));
      auto X1 = std::max(Iterator->get()->BoundingBox.left(),
                         max(OverlappingBlocks, [](const clsDocBlockPtr &b) {
                           return b->BoundingBox.left();
                         }));
      std::sort(Y.begin(), Y.end());
      DocBlockPtrVector_t NewBlocks(Y.size() - 1);
      for (size_t i = 1; i < Y.size(); ++i) {
        NewBlocks[i].reset(new stuDocTextBlock());
        NewBlocks[i]->BoundingBox = stuBoundingBox(X0, Y[i - 1], X1, Y[i]);
      }

      for (auto &Line : AllLines) {
        clsDocBlockPtr BestBlock;
        float BestOverlap = 0.f;
        for (auto &Block : NewBlocks) {
          float BlockOverlap =
              Block->BoundingBox.verticalOverlap(Line->BoundingBox);
          if (BlockOverlap > BestOverlap) {
            BestBlock = Block;
            BestOverlap = BlockOverlap;
          }
        }
        BestBlock.asText()->Lines.push_back(Line);
      }

      for(auto& Block: NewBlocks)
        if(Block.asText()->Lines.empty())
          Block.reset();

      Iterator->reset();
      for (auto &Block : OverlappingBlocks) Block.reset();

      ++Iterator;

      for(auto& Block: NewBlocks) {
        if(Block.get() == nullptr)
          continue;
        Result.push_back(Block);
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

  if (clsPdfLaDebug::instance().isObjectRegistered(this)) {
    const auto &[PrepopulatedData, PrepopulatedDataSize] =
        clsPdfLaDebug::instance().getPageImage(this, _pageIndex);
    if (PrepopulatedData.size() > 0 && PrepopulatedDataSize == _renderSize)
      return PrepopulatedData;
  }

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

  // TODO: Deal with this magic constant
  auto Items = filter(
      this->PdfiumWrapper->getPageItems(_pageIndex), [](const DocItemPtr_t &e) {
        return true;
        return e->BoundingBox.width() * e->BoundingBox.height() >
               2.f * MIN_ITEM_SIZE * MIN_ITEM_SIZE;
      });

  auto [SortedChars, SortedFigures, MeanCharWidth, MeanCharHeight,
        WordSeparationThreshold, WhitespaceCover] =
      std::move(this->preparePreliminaryData(Items, PageSize));
  auto [Lines, Figures] = std::move(
      this->findPageLinesAndFigures(SortedChars, SortedFigures, WhitespaceCover,
                                    MeanCharWidth, MeanCharHeight, PageSize));
  auto Blocks =
      this->findTextBlocks(stuBoundingBox(stuPoint(0.f, 0.f), PageSize), Lines,
                           Figures, WhitespaceCover);

  for (const auto Item : Figures) {
    clsDocBlockPtr FigureBlock;
    FigureBlock.reset(new stuDocFigureBlock);
    FigureBlock->BoundingBox = Item->BoundingBox;
    Blocks.push_back(FigureBlock);
  }

  return Blocks;
}

void setDebugOutputPath(const std::string &_path) {
  clsPdfLaDebug::instance().setDebugOutputPath(_path);
}

}  // namespace PDFLA
}  // namespace Targoman
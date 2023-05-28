#include "pdfla.h"

#include <algorithm>
#include <iostream>

#include "algorithm.hpp"
#include "clsPdfiumWrapper.h"
#include "debug.h"

namespace Targoman {
namespace PDFLA {
using namespace Targoman::DLA;
using namespace Targoman::Common;

constexpr float DEBUG_UPSCALE_FACTOR = 2.f;
constexpr float MAX_IMAGE_BLOB_AREA_FACTOR = 0.5f;

class clsPdfLaInternals {
 private:
  std::unique_ptr<clsPdfiumWrapper> PdfiumWrapper;

 private:
  float computeWordSeparationThreshold(
      const DocItemPtrVector_t &_sortedDocItems, float _width);
  BoundingBoxPtrVector_t getRawWhitespaceCover(
      const BoundingBoxPtr_t &_bounds, const BoundingBoxPtrVector_t &_obstacles,
      float _minCoverLegSize);
  BoundingBoxPtrVector_t getWhitespaceCoverage(
      const DocItemPtrVector_t &_sortedDocItems, const stuSize &_pageSize,
      float _wordSeparationThreshold);
  std::tuple<DocLinePtrVector_t, DocItemPtrVector_t> findPageLinesAndFigures(
      const DocItemPtrVector_t &_docItems, const stuSize &_pageSize);
  DocBlockPtrVector_t findPageTextBlocks(
      const DocLinePtrVector_t &_pageLines,
      const DocItemPtrVector_t &_pageFigures);

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
    const DocItemPtrVector_t &_sortedDocItems, float _width) {
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
                                     PrevItem->BoundingBox.right() + 0.5);
      if (dx >= MIN_ACKNOWLEDGABLE_DISTANCE) {
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
    return _bounds->width() >= MIN_COVER_SIZE &&
           _bounds->height() >= MIN_COVER_SIZE &&
           _bounds->width() + _bounds->height() >= MIN_COVER_PERIMETER &&
           _bounds->area() >= MIN_COVER_AREA;
  };
  auto calculateCandidateScore = [&](const BoundingBoxPtr_t &_candidate) {
    return _candidate->height() + 0.1f * _candidate->width();
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
      if (Obstacles.empty() || Score < 1) return Cover;
      auto Union = stuBoundingBox(*Obstacles.front());
      for (const auto &Obstacle : Obstacles) Union.unionWith_(Obstacle);
      auto Center = Union.center();
      auto Pivot = min(Obstacles, [&Center](const BoundingBoxPtr_t &_obstacle) {
        return -_obstacle->area();
      });
      for (auto &NewCandidate :
           {std::make_shared<stuBoundingBox>(Pivot->right(), Cover->top(),
                                             Cover->right(), Cover->bottom()),
            std::make_shared<stuBoundingBox>(Cover->left(), Cover->top(),
                                             Pivot->left(), Cover->bottom()),
            std::make_shared<stuBoundingBox>(Cover->left(), Pivot->bottom(),
                                             Cover->right(), Cover->bottom()),
            std::make_shared<stuBoundingBox>(Cover->left(), Cover->top(),
                                             Cover->right(), Pivot->top())}) {
        if (!candidateIsAcceptable(NewCandidate)) continue;
        Candidates.push_back(std::make_tuple(
            calculateCandidateScore(NewCandidate), NewCandidate,
            filter(Obstacles, [&NewCandidate](const BoundingBoxPtr_t &Item) {
              return Item->hasIntersectionWith(NewCandidate);
            })));
      }
      Candidates.erase(Candidates.begin() + ArgMax);
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
    float _wordSeparationThreshold) {
  constexpr float APPROXIMATE_FULL_OVERLAP_RATIO = 0.95f;

  DocItemPtrVector_t Blobs;
  DocItemPtr_t PrevItem = nullptr;
  float MeanCharWidth = 0;
  size_t NumberOfChars = 0;
  for (size_t i = 0; i < _sortedDocItems.size(); ++i) {
    const auto &ThisItem = _sortedDocItems.at(i);
    if (ThisItem->Type != enuDocItemType::Char) continue;
    MeanCharWidth += ThisItem->BoundingBox.width();
    ++NumberOfChars;
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
  if (NumberOfChars > 0) MeanCharWidth /= static_cast<float>(NumberOfChars);

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
      MeanCharWidth);

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

bool itemBelongsToLine(const DocItemPtr_t &_item, const DocLinePtr_t &_line) {
  float HorizontalOverlap =
      _line->BoundingBox.horizontalOverlap(_item->BoundingBox);
  if (HorizontalOverlap <= -2.5f * std::max(_item->BoundingBox.height(),
                                            _line->BoundingBox.height()))
    return false;
  float VerticalOverlap =
      _line->BoundingBox.verticalOverlap(_item->BoundingBox);
  if ((_item->BoundingBox.height() < 0.5f * _line->BoundingBox.height()) ||
      (_line->BoundingBox.height() < 0.5f * _item->BoundingBox.height())) {
    return VerticalOverlap > MIN_ITEM_SIZE;
  } else {
    return VerticalOverlap > 0.5f * std::min(_item->BoundingBox.height(),
                                             _line->BoundingBox.height());
  }
}

std::tuple<DocLinePtrVector_t, DocItemPtrVector_t>
clsPdfLaInternals::findPageLinesAndFigures(const DocItemPtrVector_t &_docItems,
                                           const stuSize &_pageSize) {
  auto [SortedFigures, SortedChars] =
      std::move(split(_docItems, [&](const DocItemPtr_t &e) {
        return e->Type != enuDocItemType::Char;
      }));
  std::sort(
      SortedFigures.begin(), SortedFigures.end(),
      [](const DocItemPtr_t &a, const DocItemPtr_t &b) {
        if (a->BoundingBox.verticalOverlap(b->BoundingBox) > MIN_ITEM_SIZE)
          return a->BoundingBox.left() < b->BoundingBox.left();
        return a->BoundingBox.top() < b->BoundingBox.top();
      });

  std::sort(SortedChars.begin(), SortedChars.end(),
            [](const DocItemPtr_t &a, const DocItemPtr_t &b) {
              return a->BoundingBox.top() < b->BoundingBox.top();
            });
  
  for(size_t Y = 0; Y < static_cast<size_t>(_pageSize.Height); ++Y) {
    auto Subset = filter(SortedChars, [&](const DocItemPtr_t &e) { return e->BoundingBox.top() <= Y && e->BoundingBox.bottom() >= Y; });
    if(Subset.size() == 0)
      continue;
    for(size_t i = 1; i < Subset.size(); ++i) {
      size_t Position = i;
      for(size_t j = i; j > 0; --j) {
        if(Subset[i]->BoundingBox.left() > Subset[j - 1]->BoundingBox.left())
          Position = j - 1;
      }
      if(Position != i) {
        auto Copy = Subset[i];
        for(size_t j = i; j > Position; --j)
          Subset[j] = Subset[j - 1];
        Subset[Position] = Copy;
      }
    }
    for(size_t i = 1; i < Subset.size(); ++i) {
      if(Subset[i]->BoundingBox.left() > Subset[i - 1]->BoundingBox.left()) {
        std::cout << "TROUBLE" << std::endl;
      }
      if(Subset[i]->BoundingBox.left() == Subset[i - 1]->BoundingBox.left()) {
        std::cout << "POSSIBLE TROUBLE" << std::endl;
      }
    }
  }

  auto WordSeparationThreshold =
      this->computeWordSeparationThreshold(SortedChars, _pageSize.Width);
  auto WhitespaceCover = this->getWhitespaceCoverage(
      cat(SortedChars, SortedFigures), _pageSize, WordSeparationThreshold);

  DocItemPtrVector_t ResultFigures;
  for (const auto &Item : SortedFigures) {
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
  DocLinePtrVector_t ResultLines;
  for (const auto &Item : SortedChars) {
    DocLinePtr_t Line = nullptr;
    for (auto &ResultItem : ResultLines)
      if (itemBelongsToLine(Item, ResultItem)) {
        auto Union = ResultItem->BoundingBox.unionWith(Item->BoundingBox);
        bool OverlapsWithCover = false;
        for (const auto &CoverItem : WhitespaceCover)
          if (CoverItem->hasIntersectionWith(Union) &&
              CoverItem->verticalOverlap(Union) > 3) {
            OverlapsWithCover = true;
            break;
          }
        if (OverlapsWithCover) continue;
        Line = ResultItem;
      }
    if (Line.get() == nullptr) {
      clsPdfLaDebug::instance().showDebugImage(
          this, "Item NO LINE", DEBUG_UPSCALE_FACTOR, ResultLines,
          DocItemPtrVector_t{Item});
      std::cout << "Bounds: (" << Item->BoundingBox.left() << ","
                << Item->BoundingBox.top() << "," << Item->BoundingBox.right()
                << "," << Item->BoundingBox.bottom() << ")" << std::endl;
    }
    // else
    //   clsPdfLaDebug::instance().showDebugImage(this, "ITEM WITH LINE",
    //   DEBUG_UPSCALE_FACTOR, ResultLines, DocLinePtrVector_t {Line},
    //   DocItemPtrVector_t {Item});

    if (Line.get() == nullptr) {
      Line = std::make_shared<stuDocLine>();
      Line->BoundingBox = Item->BoundingBox;
      ResultLines.push_back(Line);
    }
    Line->BoundingBox.unionWith_(Item->BoundingBox);
    Line->Items.push_back(Item);
  }
  return std::make_tuple(ResultLines, ResultFigures);
}

DocBlockPtrVector_t clsPdfLaInternals::findPageTextBlocks(
    const DocLinePtrVector_t &_pageLines,
    const DocItemPtrVector_t &_pageFigures) {
  auto SortedLines = _pageLines;
  std::sort(SortedLines.begin(), SortedLines.end(),
            [&](const DocLinePtr_t &a, const DocLinePtr_t &b) {
              if (a->BoundingBox.horizontalOverlap(b->BoundingBox))
                return a->BoundingBox.top() < b->BoundingBox.top();
              return a->BoundingBox.left() < b->BoundingBox.left();
            });

  clsPdfLaDebug::instance().showDebugImage(this, "LINES", DEBUG_UPSCALE_FACTOR,
                                           SortedLines);

  DocBlockPtrVector_t Result;
  for (auto &Line : SortedLines) {
    clsDocBlockPtr Block{nullptr};
    for (auto &ResultItem : Result)
      if (ResultItem->BoundingBox.horizontalOverlap(Line->BoundingBox) >= 5) {
        auto Union = ResultItem->BoundingBox.unionWith(Line->BoundingBox);
        bool Blocked = false;
        for (auto &PossibleBlocker : _pageFigures) {
          if (Union.hasIntersectionWith(PossibleBlocker->BoundingBox)) {
            Blocked = true;
            break;
          }
        }
        if (!Blocked) {
          for (auto &PossibleBlocker : SortedLines) {
            if (PossibleBlocker.get() == Line.get()) continue;
            if (PossibleBlocker->BoundingBox.horizontalOverlap(
                    Line->BoundingBox) > Line->BoundingBox.height() &&
                PossibleBlocker->BoundingBox.horizontalOverlap(
                    ResultItem->BoundingBox) > Line->BoundingBox.height())
              continue;
            bool InsideThisResultItem = false;
            for (auto &ResultItemLine : ResultItem.asText()->Lines)
              if (PossibleBlocker.get() == ResultItemLine.get()) {
                InsideThisResultItem = true;
                break;
              }
            if (InsideThisResultItem) continue;
            if (Union.hasIntersectionWith(PossibleBlocker->BoundingBox)) {
              Blocked = true;
              break;
            }
          }
        }
        if (!Blocked) {
          Block = ResultItem;
          break;
        }
      }
    if (Block.get() == nullptr) {
      Block.reset(new stuDocTextBlock);
      Block->BoundingBox = Line->BoundingBox;
      Result.push_back(Block);
    }
    Block->BoundingBox.unionWith_(Line->BoundingBox);
    Block.asText()->Lines.push_back(Line);
  }
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
  clsPdfLaDebug::instance().setCurrentPageIndex(this, _pageIndex);

  auto PageSize = this->getPageSize(_pageIndex);

  if (clsPdfLaDebug::instance().isObjectRegister(this)) {
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
  auto [Lines, Figures] =
      std::move(this->findPageLinesAndFigures(Items, PageSize));
  auto Blocks = this->findPageTextBlocks(Lines, Figures);
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
  auto [Lines, Figures] =
      std::move(this->findPageLinesAndFigures(Items, PageSize));
  return this->findPageTextBlocks(Lines, Figures);
}

void setDebugOutputPath(const std::string &_path) {
  clsPdfLaDebug::instance().setDebugOutputPath(_path);
}

}  // namespace PDFLA
}  // namespace Targoman
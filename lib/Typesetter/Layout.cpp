#include "Layout.h"

#include "Kinsoku.h"

#include <algorithm>

namespace ts {

void PageLayouter::begin(const LayoutOptions& options) {
  opt = options;
  if (opt.em < 1) {
    opt.em = 1;
  }
  rubyGutter = opt.compactColumns && !opt.hasRuby ? static_cast<int16_t>(std::max(1, opt.em / 5))
                                                  : static_cast<int16_t>((opt.em + 1) / 2);
  pitch = static_cast<int16_t>(opt.em + rubyGutter);
  const int16_t innerW = static_cast<int16_t>(opt.width - opt.margin * 2);
  const int16_t innerH = static_cast<int16_t>(opt.height - opt.margin * 2);

  currentCount = 0;
  committedCount = 0;
  pageReady = false;
  lastAtomPos = 0;
  firstOnCurrent = 0;
  feedPos = 0;

  if (opt.mode == WritingMode::VerticalRl) {
    const int nCols = pitch > 0 ? innerW / pitch : 0;
    const int nChars = opt.em > 0 ? innerH / opt.em : 0;
    if (nCols < 1 || nChars < 1) {
      return;
    }
    const int16_t usedW = static_cast<int16_t>(nCols * pitch);
    const int16_t usedH = static_cast<int16_t>(nChars * opt.em);
    const int16_t padLeft = static_cast<int16_t>((innerW - usedW) / 2);
    const int16_t padTop = static_cast<int16_t>((innerH - usedH) / 2);
    colTop = static_cast<int16_t>(opt.margin + padTop);
    colBottom = static_cast<int16_t>(colTop + usedH);
    hanmenLeft = static_cast<int16_t>(opt.margin + padLeft);
    firstColX = static_cast<int16_t>(hanmenLeft + usedW - pitch);
  } else {
    const int nChars = opt.em > 0 ? innerW / opt.em : 0;
    const int nLines = pitch > 0 ? innerH / pitch : 0;
    if (nChars < 1 || nLines < 1) {
      return;
    }
    const int16_t usedW = static_cast<int16_t>(nChars * opt.em);
    const int16_t usedH = static_cast<int16_t>(nLines * pitch);
    const int16_t padLeft = static_cast<int16_t>((innerW - usedW) / 2);
    const int16_t padTop = static_cast<int16_t>((innerH - usedH) / 2);
    firstLineY = static_cast<int16_t>(opt.margin + padTop);
    hanmenLeft = static_cast<int16_t>(opt.margin + padLeft);
    lineRight = static_cast<int16_t>(hanmenLeft + usedW);
    hanmenBottom = static_cast<int16_t>(firstLineY + usedH);
  }
  resetCursor();
}

void PageLayouter::resetCursor() {
  if (opt.mode == WritingMode::VerticalRl) {
    indent = colTop;
    colX = firstColX;
    y = indent;
  } else {
    indent = hanmenLeft;
    lineY = firstLineY;
    x = indent;
  }
}

void PageLayouter::emit(const GlyphRun& run, const uint32_t pos) {
  if (currentCount >= kMaxGlyphs) {
    newPage();
  }
  if (currentCount == 0) {
    firstOnCurrent = pos;
  }
  if (currentCount < kMaxGlyphs) {
    current[currentCount++] = run;
  }
}

void PageLayouter::newPage() {
  committedCount = currentCount;
  for (uint16_t i = 0; i < currentCount; ++i) {
    committed[i] = current[i];
  }
  currentCount = 0;
  firstOnCurrent = 0;
  resetCursor();
  pageReady = true;
}

void PageLayouter::newColumn() {
  colX = static_cast<int16_t>(colX - pitch);
  y = indent;
  if (colX < hanmenLeft) {
    newPage();
  }
}

void PageLayouter::newLine() {
  lineY = static_cast<int16_t>(lineY + pitch);
  x = indent;
  if (lineY + pitch > hanmenBottom) {
    newPage();
  }
}

void PageLayouter::placeChar(const uint32_t cp, const uint32_t* ruby, const uint8_t rubyCount, const bool tcy) {
  const int16_t F = opt.em;
  if (opt.mode == WritingMode::VerticalRl) {
    if (y + F > colBottom) {
      if (!kinsokuCanStartColumn(cp) && currentCount > 0) {
        const GlyphRun& last = current[currentCount - 1];
        if (last.x == colX) {
          GlyphRun pulled = last;
          --currentCount;
          if (currentCount == 0) {
            firstOnCurrent = 0;
          }
          newColumn();
          pulled.x = colX;
          pulled.y = y;
          emit(pulled, lastAtomPos);
          y = static_cast<int16_t>(y + F);
        } else {
          newColumn();
        }
      } else {
        newColumn();
      }
    }
    if (y + F > colBottom) {
      newColumn();
    }
    GlyphRun run{};
    run.cp = cp;
    run.rubyCount = rubyCount;
    if (ruby && rubyCount > 0) {
      const uint8_t n = rubyCount > 4 ? 4 : rubyCount;
      for (uint8_t i = 0; i < n; ++i) {
        run.ruby[i] = ruby[i];
      }
    }
    run.x = colX;
    run.y = y;
    run.size = F;
    if (!tcy && shouldRotate(cp)) {
      run.flags = 1;
    }
    if (tcy) {
      run.flags = static_cast<uint8_t>(run.flags | 2);
    }
    emit(run, feedPos);
    y = static_cast<int16_t>(y + F);
    return;
  }

  if (x + F > lineRight) {
    if (!kinsokuCanStartColumn(cp) && currentCount > 0) {
      const GlyphRun& last = current[currentCount - 1];
      if (last.y == glyphY()) {
        GlyphRun pulled = last;
        --currentCount;
        if (currentCount == 0) {
          firstOnCurrent = 0;
        }
        newLine();
        pulled.x = x;
        pulled.y = glyphY();
        emit(pulled, lastAtomPos);
        x = static_cast<int16_t>(x + F);
      } else {
        newLine();
      }
    } else {
      newLine();
    }
  }
  if (x + F > lineRight) {
    newLine();
  }
  GlyphRun run{};
  run.cp = cp;
  run.rubyCount = rubyCount;
  if (ruby && rubyCount > 0) {
    const uint8_t n = rubyCount > 4 ? 4 : rubyCount;
    for (uint8_t i = 0; i < n; ++i) {
      run.ruby[i] = ruby[i];
    }
  }
  run.x = x;
  run.y = glyphY();
  run.size = F;
  run.flags = 4;  // rubyAbove
  emit(run, feedPos);
  x = static_cast<int16_t>(x + F);
}

bool PageLayouter::feed(const Atom& atom, const uint32_t atomPos) {
  pageReady = false;
  feedPos = atomPos;
  switch (atom.kind) {
    case AtomKind::PageBreak:
      if (currentCount > 0) {
        newPage();
      }
      resetCursor();
      if (opt.mode == WritingMode::VerticalRl) {
        indent = colTop;
        y = colTop;
      } else {
        indent = hanmenLeft;
        x = indent;
        lineY = firstLineY;
      }
      break;
    case AtomKind::ColumnBreak: {
      if (opt.mode == WritingMode::VerticalRl) {
        const int16_t next = std::min(static_cast<int16_t>(colTop + atom.startEm * opt.em),
                                      static_cast<int16_t>(colBottom - opt.em));
        if (y > indent) {
          indent = next;
          newColumn();
        } else {
          indent = next;
          y = indent;
        }
      } else {
        const int16_t next = std::min(static_cast<int16_t>(hanmenLeft + atom.startEm * opt.em),
                                      static_cast<int16_t>(lineRight - opt.em));
        if (x > indent) {
          indent = next;
          newLine();
        } else {
          indent = next;
          x = indent;
        }
      }
      break;
    }
    case AtomKind::Space: {
      const int16_t w = static_cast<int16_t>((opt.em * 35) / 100);
      if (opt.mode == WritingMode::VerticalRl) {
        if (y == indent) {
          break;
        }
        if (y + w + opt.em > colBottom) {
          newColumn();
        } else {
          y = static_cast<int16_t>(y + w);
        }
      } else {
        if (x <= indent) {
          break;
        }
        if (x + w + opt.em > lineRight) {
          newLine();
        } else {
          x = static_cast<int16_t>(x + w);
        }
      }
      break;
    }
    case AtomKind::Gap:
      if (opt.mode == WritingMode::VerticalRl) {
        y = static_cast<int16_t>(y + opt.em / 2);
        if (y + opt.em > colBottom) {
          newColumn();
        }
      } else if (x > indent) {
        newLine();
      }
      break;
    case AtomKind::Tcy:
      if (opt.mode == WritingMode::HorizontalTb) {
        const int need = opt.em * (atom.tcyCount > 0 ? atom.tcyCount : 1);
        if (x + need > lineRight && x > indent) {
          newLine();
        }
        const uint8_t n = atom.tcyCount > 0 ? atom.tcyCount : 1;
        for (uint8_t i = 0; i < n; ++i) {
          placeChar(i == 0 ? atom.cp : atom.tcy[i], nullptr, 0, false);
        }
      } else {
        placeChar(atom.cp, nullptr, 0, true);
      }
      lastAtomPos = atomPos;
      break;
    case AtomKind::Ch:
      placeChar(atom.cp, atom.ruby, atom.rubyCount, false);
      lastAtomPos = atomPos;
      break;
  }
  return pageReady;
}

bool PageLayouter::finish() {
  if (currentCount == 0) {
    return false;
  }
  newPage();
  return true;
}

RubyAlong placeRubyAlong(const int16_t origin, const int16_t em, const int16_t rubyEm, const uint8_t count,
                         const bool loEmpty, const bool hiEmpty) {
  RubyAlong out{};
  if (count == 0 || em <= 0 || rubyEm <= 0) {
    return out;
  }
  int lo = origin;
  int hi = origin + em;
  if (loEmpty) {
    lo -= em;
  }
  if (hiEmpty) {
    hi += em;
  }
  int pitch = rubyEm;
  int span = static_cast<int>(count) * pitch;
  const int room = hi - lo;
  if (span > room) {
    pitch = room / count;
    if (pitch < 1) {
      pitch = 1;
    }
    span = static_cast<int>(count) * pitch;
  }
  int start = origin + (em - span) / 2;
  if (start < lo) {
    start = lo;
  }
  if (start + span > hi) {
    start = hi - span;
  }
  out.start = static_cast<int16_t>(start);
  out.pitch = static_cast<int16_t>(pitch);
  return out;
}

}  // namespace ts

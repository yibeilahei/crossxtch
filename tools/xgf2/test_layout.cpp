// Host test for em-grid 縦書き + 禁則. Compile:
//   c++ -std=c++20 -I lib/Typesetter tools/xgf2/test_layout.cpp lib/Typesetter/Layout.cpp lib/Typesetter/Kinsoku.cpp -o /tmp/test_layout && /tmp/test_layout

#include "Kinsoku.h"
#include "Layout.h"

#include <cstdio>
#include <cstdlib>

static void fail(const char* msg) {
  std::fprintf(stderr, "FAIL %s\n", msg);
  std::exit(1);
}

int main() {
  if (ts::kinsokuCanStartColumn(0x3042) == false) {
    fail("あ should start a column");
  }
  if (ts::kinsokuCanStartColumn(0x3002) == true) {
    fail("。 must not start a column");
  }
  if (ts::kinsokuCanEndColumn(0x300C) == true) {
    fail("「 must not end a column");
  }
  if (!ts::shouldRotate('A') || ts::shouldRotate(0x4E00)) {
    fail("rotate ASCII not 一");
  }

  ts::LayoutOptions opt{};
  opt.width = 70;  // one column at em=20, ruby gutter 10, pitch 30 → 70/30 = 2 cols
  opt.height = 80;
  opt.em = 20;
  opt.margin = 0;
  opt.hasRuby = true;
  opt.mode = ts::WritingMode::VerticalRl;

  ts::PageLayouter lay;
  lay.begin(opt);

  int pages = 0;
  // 8 chars: 2 cols x 4 chars (80/20=4)
  for (int i = 0; i < 8; ++i) {
    ts::Atom a{};
    a.kind = ts::AtomKind::Ch;
    a.cp = 0x3042 + static_cast<uint32_t>(i % 5);
    if (lay.feed(a, static_cast<uint32_t>(i))) {
      ++pages;
      lay.clearPage();
    }
  }
  if (lay.finish()) {
    ++pages;
  }
  if (pages < 1) {
    fail("expected at least one page");
  }

  // 禁則: a column of 4 that would start the next with 。 should pull back.
  lay.begin(opt);
  pages = 0;
  const uint32_t seq[] = {0x3042, 0x3044, 0x3046, 0x3048, 0x3002};  // あいうえ。 wraps; 。 cannot start a column
  uint32_t seqPos = 0;
  for (uint32_t cp : seq) {
    ts::Atom a{};
    a.kind = ts::AtomKind::Ch;
    a.cp = cp;
    if (lay.feed(a, seqPos++)) {
      ++pages;
      lay.clearPage();
    }
  }
  lay.finish();
  bool foundKinsoku = false;
  for (uint16_t i = 0; i < lay.pageGlyphCount(); ++i) {
    if (lay.page()[i].cp == 0x3002) {
      foundKinsoku = true;
    }
  }
  if (!foundKinsoku && pages == 0) {
    fail("。 dropped");
  }

  std::printf("ok pages=%d glyphs=%u\n", pages, lay.pageGlyphCount());

  auto expectRuby = [](const ts::RubyAlong& got, int start, int pitch, const char* name) {
    if (got.start != start || got.pitch != pitch) {
      std::fprintf(stderr, "FAIL %s start=%d pitch=%d (want %d %d)\n", name, got.start, got.pitch, start, pitch);
      std::exit(1);
    }
  };
  // em=20 rubyEm=10 origin=100
  expectRuby(ts::placeRubyAlong(100, 20, 10, 2, true, true), 100, 10, "2 kana fills cell");
  expectRuby(ts::placeRubyAlong(100, 20, 10, 1, true, true), 105, 10, "1 kana centered");
  expectRuby(ts::placeRubyAlong(100, 20, 10, 4, true, true), 90, 10, "4 kana overhang both empty");
  expectRuby(ts::placeRubyAlong(100, 20, 10, 4, false, false), 100, 5, "4 kana clamp both ruby");
  expectRuby(ts::placeRubyAlong(100, 20, 10, 3, false, true), 100, 10, "3 kana shift into empty next");
  expectRuby(ts::placeRubyAlong(100, 20, 10, 3, true, false), 90, 10, "3 kana shift into empty prev");
  expectRuby(ts::placeRubyAlong(100, 20, 10, 4, false, true), 100, 10, "4 kana only next empty");

  return 0;
}

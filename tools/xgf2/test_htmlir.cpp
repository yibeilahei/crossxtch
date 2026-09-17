// c++ -std=c++20 -I lib/Typesetter -I lib/Utf8 \
//   tools/xgf2/test_htmlir.cpp lib/Typesetter/HtmlSax.cpp lib/Typesetter/HtmlIR.cpp \
//   lib/Utf8/Utf8.cpp -o /tmp/test_htmlir && /tmp/test_htmlir

#include "HtmlIR.h"

#include <Utf8.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using ts::Atom;
using ts::AtomKind;

struct Dump {
  std::vector<Atom> atoms;
};

static bool collect(void* ctx, const Atom& a, uint32_t) {
  static_cast<Dump*>(ctx)->atoms.push_back(a);
  return true;
}

static std::vector<Atom> parse(const char* xml) {
  std::string wrapped = xml;
  if (wrapped.find("<html") == std::string::npos) {
    wrapped = std::string("<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>") + xml + "</body></html>";
  }
  Dump d;
  ts::AtomSink sink{&d, collect};
  ts::htmlToAtoms(wrapped.c_str(), wrapped.size(), sink, nullptr);
  // drop leading/trailing gaps like lazahata trimGaps
  while (!d.atoms.empty() && (d.atoms.front().kind == AtomKind::Gap || d.atoms.front().kind == AtomKind::Space ||
                              d.atoms.front().kind == AtomKind::ColumnBreak)) {
    d.atoms.erase(d.atoms.begin());
  }
  while (!d.atoms.empty() && (d.atoms.back().kind == AtomKind::Gap || d.atoms.back().kind == AtomKind::Space ||
                              d.atoms.back().kind == AtomKind::ColumnBreak)) {
    d.atoms.pop_back();
  }
  return d.atoms;
}

static void fail(const char* msg) {
  std::fprintf(stderr, "FAIL %s\n", msg);
  std::exit(1);
}

static bool isCh(const Atom& a, uint32_t cp, const char* rubyUtf8) {
  if (a.kind != AtomKind::Ch || a.cp != cp) {
    return false;
  }
  if (!rubyUtf8 || !*rubyUtf8) {
    return a.rubyCount == 0;
  }
  const unsigned char* p = reinterpret_cast<const unsigned char*>(rubyUtf8);
  uint8_t n = 0;
  uint32_t cps[4]{};
  while (*p && n < 4) {
    cps[n++] = utf8NextCodepoint(&p);
  }
  if (n != a.rubyCount) {
    return false;
  }
  for (uint8_t i = 0; i < n; ++i) {
    if (cps[i] != a.ruby[i]) {
      return false;
    }
  }
  return true;
}

int main() {
  {
    auto a = parse("<p><ruby><rb>吞</rb><rt>の</rt></ruby></p>");
    if (a.size() != 1 || !isCh(a[0], 0x541E, "の")) {
      fail("ruby rb");
    }
  }
  {
    auto a = parse("<p><ruby>鷲<rt>わし</rt>摑<rt>づか</rt></ruby></p>");
    if (a.size() != 2 || !isCh(a[0], 0x9DF2, "わし") || !isCh(a[1], 0x6451, "づか")) {
      fail("jukugo");
    }
  }
  {
    auto a = parse("<p>4<span class=\"tcy\">kg</span></p>");
    if (a.size() != 2 || a[0].cp != '4' || a[1].kind != AtomKind::Tcy || a[1].cp != 'k') {
      fail("tcy");
    }
  }
  {
    auto a = parse("<p><img class=\"gaiji\" src=\"x.jpeg\" alt=\"→\"/></p>");
    if (a.size() != 1 || a[0].cp != 0x2192) {
      fail("gaiji alt");
    }
  }
  {
    auto a = parse("<p>\n  アナログ\n</p>");
    if (a.size() != 4) {
      fail("pretty print");
    }
  }
  {
    auto a = parse("<p>vitti 'na crozza</p>");
    bool space = false;
    for (auto& x : a) {
      if (x.kind == AtomKind::Space) {
        space = true;
      }
    }
    if (!space) {
      fail("ascii space");
    }
  }
  {
    // 気 at SAX chunk boundary used to split E6 B0 97 and drop the kanji.
    std::string body;
    for (int i = 0; i < 60; ++i) {
      body += "あ";  // 180 UTF-8 bytes, then 気 straddles the old 184 cap
    }
    body += "気の置けない友人";
    std::string html = "<p>" + body + "</p>";
    auto a = parse(html.c_str());
    bool found = false;
    for (auto& x : a) {
      if (x.kind == AtomKind::Ch && x.cp == 0x6C17) {
        found = true;
      }
    }
    if (!found) {
      fail("気 split across text chunk");
    }
  }
  std::printf("ok htmlir %s\n", "ruby/tcy/gaiji");
  return 0;
}

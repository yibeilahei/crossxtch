#include "HtmlIR.h"

#include "HtmlSax.h"

#include <Utf8.h>

#include <cstdio>
#include <cstring>

namespace ts {
namespace {

bool emitAtom(const AtomSink& sink, const Atom& a, const uint32_t pos) {
  if (!sink.emit) {
    return true;
  }
  return sink.emit(sink.ctx, a, pos);
}

bool eq(const char* a, const char* b) { return a && b && strcmp(a, b) == 0; }

bool isSkip(const char* name) {
  return eq(name, "script") || eq(name, "style") || eq(name, "head") || eq(name, "rp");
}

bool isPara(const char* name) {
  return eq(name, "p") || eq(name, "li") || eq(name, "tr") ||
         (name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == 0);
}

bool isHeading(const char* name) { return name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == 0; }

int startEmFromClass(const HtmlSax& sax) {
  char cls[256];
  if (!sax.attr("class", cls, sizeof(cls))) {
    return -1;
  }
  const char* s = cls;
  while (*s) {
    while (*s == ' ') {
      ++s;
    }
    const char* e = s;
    while (*e && *e != ' ') {
      ++e;
    }
    auto take = [&](const char* prefix) -> int {
      const size_t pn = strlen(prefix);
      if (static_cast<size_t>(e - s) <= pn || memcmp(s, prefix, pn) != 0) {
        return -1;
      }
      const char* n = s + pn;
      const char* ne = e;
      if (ne - n >= 2 && ne[-2] == 'e' && ne[-1] == 'm') {
        ne -= 2;
      }
      int v = 0;
      if (n == ne) {
        return -1;
      }
      while (n < ne) {
        if (*n < '0' || *n > '9') {
          return -1;
        }
        v = v * 10 + (*n - '0');
        ++n;
      }
      if (v >= 0 && v <= 20) {
        return v;
      }
      return -1;
    };
    int v = take("h-indent-");
    if (v < 0) {
      v = take("start-");
    }
    if (v >= 0) {
      return v;
    }
    s = e;
  }
  return -1;
}

WritingMode modeFromSax(const HtmlSax& sax) {
  if (sax.hasClass("hltr") || sax.hasClassPrefix("hltr") || sax.hasClass("horizontal-tb")) {
    return WritingMode::HorizontalTb;
  }
  if (sax.hasClass("vrtl") || sax.hasClassPrefix("vrtl") || sax.hasClass("vertical-rl")) {
    return WritingMode::VerticalRl;
  }
  char style[192];
  if (sax.attr("style", style, sizeof(style))) {
    if (strstr(style, "horizontal-tb") || strstr(style, "lr-tb") || strstr(style, "rl-tb")) {
      return WritingMode::HorizontalTb;
    }
    if (strstr(style, "vertical-rl") || strstr(style, "tb-rl") || strstr(style, "vertical-lr")) {
      return WritingMode::VerticalRl;
    }
  }
  return WritingMode::VerticalRl;
}

bool modeSpecified(const HtmlSax& sax) {
  return sax.hasClass("hltr") || sax.hasClassPrefix("hltr") || sax.hasClass("horizontal-tb") || sax.hasClass("vrtl") ||
         sax.hasClassPrefix("vrtl") || sax.hasClass("vertical-rl") || sax.attrContains("style", "writing-mode") ||
         sax.attrContains("style", "horizontal-tb") || sax.attrContains("style", "vertical-rl");
}

void appendUtf8(uint32_t* cps, uint8_t& n, const uint8_t cap, const char* s, const uint16_t slen) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
  const unsigned char* end = p + slen;
  while (p < end && n < cap) {
    const unsigned char* next = p;
    const uint32_t cp = utf8NextCodepoint(&next);
    if (next == p) {
      break;
    }
    p = next;
    if (cp == 0) {
      break;
    }
    cps[n++] = cp;
  }
}

bool isWsCp(const uint32_t cp) { return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == 0xA0; }

Atom chAtom(const uint32_t cp, const uint32_t* ruby, const uint8_t rubyN) {
  Atom a{};
  a.kind = AtomKind::Ch;
  a.cp = cp;
  a.rubyCount = rubyN > 4 ? 4 : rubyN;
  for (uint8_t i = 0; i < a.rubyCount; ++i) {
    a.ruby[i] = ruby[i];
  }
  return a;
}

}  // namespace

void splitReading(const uint32_t* rt, const uint8_t rtN, const uint8_t baseN, uint32_t* out, uint8_t* outCount) {
  for (uint8_t i = 0; i < baseN; ++i) {
    outCount[i] = 0;
  }
  if (baseN == 0) {
    return;
  }
  if (rtN == baseN) {
    for (uint8_t i = 0; i < baseN; ++i) {
      out[i * 4] = rt[i];
      outCount[i] = 1;
    }
    return;
  }
  const uint8_t n = rtN > 4 ? 4 : rtN;
  outCount[0] = n;
  for (uint8_t i = 0; i < n; ++i) {
    out[i] = rt[i];
  }
}

static bool htmlFromSax(HtmlSax& sax, const AtomSink sink, HtmlIRResult* result);

bool htmlToAtoms(const char* data, const size_t len, const AtomSink sink, HtmlIRResult* result) {
  HtmlSax sax;
  sax.bind(data, len);
  return htmlFromSax(sax, sink, result);
}

bool htmlToAtomsPull(int (*read)(void*, char*, int), void* ctx, const AtomSink sink, HtmlIRResult* result) {
  HtmlSax sax;
  sax.bindPull(read, ctx);
  return htmlFromSax(sax, sink, result);
}

static bool htmlFromSax(HtmlSax& sax, const AtomSink sink, HtmlIRResult* result) {
  HtmlIRResult local{};
  HtmlIRResult& res = result ? *result : local;

  bool tocLike = false;
  bool inBody = false;
  int skipDepth = 0;
  int rubyDepth = 0;
  int tcyDepth = 0;
  int indent = 0;
  int indentStack[16]{};
  uint8_t indentTop = 0;
  uint32_t tcyCps[4]{};
  uint8_t tcyN = 0;
  uint32_t rubyBase[8]{};
  uint8_t rubyBaseN = 0;
  uint32_t rubyRt[8]{};
  uint8_t rubyRtN = 0;
  bool inRt = false;
  bool inRb = false;
  bool lastWasGap = true;
  bool lastWasCol = false;
  bool lastWasGlyph = false;
  uint32_t textCount = 0;
  char titleBuf[96]{};
  bool capturingTitle = false;
  uint8_t titleN = 0;

  auto emit = [&](const Atom& a, const uint32_t pos) -> bool {
    if (a.kind == AtomKind::Gap) {
      if (lastWasGap || lastWasCol) {
        return true;
      }
      lastWasGap = true;
      lastWasGlyph = false;
    } else if (a.kind == AtomKind::ColumnBreak) {
      lastWasCol = true;
      lastWasGap = false;
      lastWasGlyph = false;
    } else if (a.kind == AtomKind::Space) {
      lastWasGlyph = false;
    } else {
      lastWasGap = false;
      lastWasCol = false;
      lastWasGlyph = a.kind == AtomKind::Ch || a.kind == AtomKind::Tcy;
    }
    return emitAtom(sink, a, pos);
  };

  auto flushRubyPair = [&](const uint32_t pos) {
    if (rubyBaseN == 0) {
      rubyRtN = 0;
      return;
    }
    uint32_t packed[8 * 4]{};
    uint8_t counts[8]{};
    splitReading(rubyRt, rubyRtN, rubyBaseN, packed, counts);
    for (uint8_t i = 0; i < rubyBaseN; ++i) {
      emit(chAtom(rubyBase[i], packed + i * 4, counts[i]), pos);
    }
    rubyBaseN = 0;
    rubyRtN = 0;
  };

  auto emitText2 = [&](const char* s, const uint16_t n, const uint32_t pos) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
    const unsigned char* end = p + n;
    while (p < end) {
      const unsigned char* next = p;
      const uint32_t cp = utf8NextCodepoint(&next);
      if (next == p) {
        break;
      }
      p = next;
      if (cp == 0 || cp == '\n' || cp == '\r' || cp == '\t') {
        continue;
      }
      if (skipDepth > 0) {
        continue;
      }
      if (rubyDepth > 0) {
        if (inRt) {
          if (rubyRtN < 8) {
            rubyRt[rubyRtN++] = cp;
          }
        } else if (!isWsCp(cp)) {
          if (rubyBaseN < 8) {
            rubyBase[rubyBaseN++] = cp;
          }
        }
        continue;
      }
      if (tcyDepth > 0) {
        if (!isWsCp(cp) && tcyN < 4) {
          tcyCps[tcyN++] = cp;
        }
        continue;
      }
      if (cp == ' ' || cp == 0xA0) {
        if (lastWasGlyph) {
          Atom sp{};
          sp.kind = AtomKind::Space;
          emit(sp, pos);
        }
        continue;
      }
      ++textCount;
      if (capturingTitle) {
        unsigned char tmp[4];
        int tn = 0;
        if (cp < 0x80) {
          tmp[tn++] = static_cast<unsigned char>(cp);
        } else if (cp < 0x800) {
          tmp[tn++] = static_cast<unsigned char>(0xC0 | (cp >> 6));
          tmp[tn++] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
          tmp[tn++] = static_cast<unsigned char>(0xE0 | (cp >> 12));
          tmp[tn++] = static_cast<unsigned char>(0x80 | ((cp >> 6) & 0x3F));
          tmp[tn++] = static_cast<unsigned char>(0x80 | (cp & 0x3F));
        }
        if (titleN + tn < sizeof(titleBuf) - 1) {
          memcpy(titleBuf + titleN, tmp, static_cast<size_t>(tn));
          titleN = static_cast<uint8_t>(titleN + tn);
          titleBuf[titleN] = 0;
        }
      }
      emit(chAtom(cp, nullptr, 0), pos);
    }
  };
  (void)appendUtf8;

  while (sax.next()) {
    if (sax.kind == HtmlSax::Kind::Text) {
      emitText2(sax.text, sax.ntext, sax.pos);
      continue;
    }
    if (sax.kind == HtmlSax::Kind::Start || sax.kind == HtmlSax::Kind::Empty) {
      if (eq(sax.name, "html") || eq(sax.name, "body")) {
        if (modeSpecified(sax)) {
          res.mode = modeFromSax(sax);
          res.hasMode = true;
        }
        if (eq(sax.name, "body")) {
          inBody = true;
          if (sax.hasClass("p-toc") || sax.hasClass("toc") || sax.hasClassPrefix("p-toc")) {
            tocLike = true;
          }
        }
      }
      if (eq(sax.name, "nav") && (sax.attrContains("type", "toc") || sax.hasClass("toc"))) {
        tocLike = true;
      }
      char id[64];
      if (sax.attr("id", id, sizeof(id)) && id[0]) {
        // anchors ignored for layout
      }
      if (isSkip(sax.name)) {
        if (sax.kind == HtmlSax::Kind::Start) {
          ++skipDepth;
        }
        continue;
      }
      if (skipDepth > 0) {
        continue;
      }
      const int em = startEmFromClass(sax);
      if (sax.kind == HtmlSax::Kind::Start && indentTop < 15) {
        indentStack[indentTop++] = indent;
        if (em >= 0) {
          indent = em;
        }
      } else if (em >= 0) {
        indent = em;
      }

      if (eq(sax.name, "ruby")) {
        ++rubyDepth;
        rubyBaseN = 0;
        rubyRtN = 0;
        inRt = false;
        inRb = false;
      } else if (eq(sax.name, "rt")) {
        if (rubyDepth > 0 && rubyBaseN > 0 && rubyRtN > 0) {
          flushRubyPair(sax.pos);
        }
        inRt = true;
        rubyRtN = 0;
      } else if (eq(sax.name, "rb")) {
        inRb = true;
      } else if (eq(sax.name, "br")) {
        Atom g{};
        g.kind = AtomKind::Gap;
        emit(g, sax.pos);
      } else if (eq(sax.name, "span") && (sax.hasClass("tcy") || sax.hasClassPrefix("tcy"))) {
        ++tcyDepth;
        tcyN = 0;
      } else if (isPara(sax.name) || eq(sax.name, "div")) {
        if (isHeading(sax.name) && titleBuf[0] == 0) {
          capturingTitle = true;
          titleN = 0;
        }
        if (tocLike && textCount < 2000 && isPara(sax.name)) {
          Atom c{};
          c.kind = AtomKind::ColumnBreak;
          c.startEm = static_cast<uint8_t>(indent);
          emit(c, sax.pos);
        } else if (!lastWasGap && !lastWasCol) {
          Atom g{};
          g.kind = AtomKind::Gap;
          emit(g, sax.pos);
        }
      } else if (eq(sax.name, "img") || eq(sax.name, "image")) {
        char alt[32];
        alt[0] = 0;
        sax.attr("alt", alt, sizeof(alt));
        const bool gaiji = sax.hasClass("gaiji") || sax.hasClassPrefix("gaiji");
        uint32_t altCp = 0;
        if (alt[0]) {
          const unsigned char* ap = reinterpret_cast<const unsigned char*>(alt);
          altCp = utf8NextCodepoint(&ap);
        }
        if (gaiji && altCp && static_cast<unsigned>(strlen(alt)) <= 6) {
          emit(chAtom(altCp, nullptr, 0), sax.pos);
        } else if (!gaiji) {
          Atom pb{};
          pb.kind = AtomKind::PageBreak;
          emit(pb, sax.pos);
        } else if (altCp) {
          emit(chAtom(altCp, nullptr, 0), sax.pos);
        }
      }
      continue;
    }
    if (sax.kind == HtmlSax::Kind::End) {
      if (isSkip(sax.name) && skipDepth > 0) {
        --skipDepth;
        continue;
      }
      if (eq(sax.name, "rt")) {
        inRt = false;
        if (rubyBaseN > 0) {
          flushRubyPair(sax.pos);
        }
      } else if (eq(sax.name, "rb")) {
        inRb = false;
      } else if (eq(sax.name, "ruby")) {
        if (rubyBaseN > 0) {
          flushRubyPair(sax.pos);
        }
        if (rubyDepth > 0) {
          --rubyDepth;
        }
        inRt = false;
        inRb = false;
      } else if (eq(sax.name, "span") && tcyDepth > 0) {
        --tcyDepth;
        if (tcyN > 0) {
          Atom t{};
          t.kind = AtomKind::Tcy;
          t.cp = tcyCps[0];
          t.tcyCount = tcyN;
          for (uint8_t i = 0; i < tcyN && i < 4; ++i) {
            t.tcy[i] = tcyCps[i];
          }
          emit(t, sax.pos);
        }
        tcyN = 0;
      } else if (isPara(sax.name) || eq(sax.name, "div")) {
        capturingTitle = false;
        if (tocLike && textCount < 2000 && isPara(sax.name)) {
          Atom c{};
          c.kind = AtomKind::ColumnBreak;
          c.startEm = static_cast<uint8_t>(indent);
          emit(c, sax.pos);
        } else {
          Atom g{};
          g.kind = AtomKind::Gap;
          emit(g, sax.pos);
        }
      }
      if (indentTop > 0) {
        indent = indentStack[--indentTop];
      }
    }
  }

  if (tocLike && textCount < 2000) {
    res.compactColumns = true;
  }
  if (titleBuf[0]) {
    snprintf(res.title, sizeof(res.title), "%s", titleBuf);
  }
  (void)inBody;
  return true;
}

}  // namespace ts

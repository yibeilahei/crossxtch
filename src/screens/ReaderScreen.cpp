#include "ReaderScreen.h"

#include <Gfx.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include <algorithm>

#include <HalTiltSensor.h>

#include "core/Power.h"
#include "core/Settings.h"
#include "core/fontIds.h"
#include "screens/ChapterSelectionScreen.h"

namespace {
uint32_t pathHash(const char* path) {
  uint32_t h = 2166136261u;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(path); *p; ++p) {
    h ^= *p;
    h *= 16777619u;
  }
  return h;
}

void progressPath(char* out, size_t outSize, const char* bookPath) {
  snprintf(out, outSize, "%s/p_%08lx.bin", Settings::kDir, static_cast<unsigned long>(pathHash(bookPath)));
}
}  // namespace

ReaderScreen::ReaderScreen(Gfx& gfx, MappedInput& input, const char* path) : Screen("Reader", gfx, input) {
  snprintf(bookPath, sizeof(bookPath), "%s", path ? path : "");
}

void ReaderScreen::loadProgress() {
  char p[64];
  progressPath(p, sizeof(p), bookPath);
  HalFile f;
  if (!Storage.openFileForRead("PRG", p, f)) {
    page = 0;
    LOG_INF("RDR", "No progress file, start at 0");
    return;
  }
  uint32_t saved = 0;
  if (f.read(&saved, sizeof(saved)) == static_cast<int>(sizeof(saved))) {
    page = saved;
    LOG_INF("RDR", "Progress %s -> page %lu", p, static_cast<unsigned long>(page));
  } else {
    page = 0;
    LOG_ERR("RDR", "Progress file %s unreadable", p);
  }
}

void ReaderScreen::saveProgress() const {
  Storage.ensureDirectoryExists(Settings::kDir);
  char p[64];
  progressPath(p, sizeof(p), bookPath);
  HalFile f;
  if (!Storage.openFileForWrite("PRG", p, f)) {
    LOG_ERR("PRG", "Could not write %s", p);
    return;
  }
  f.write(&page, sizeof(page));
}

void ReaderScreen::onEnter() {
  Screen::onEnter();
  pagesUntilFull = settings.refreshEveryNPages;
  if (book.open(bookPath) != xtch::Error::Ok) {
    LOG_ERR("RDR", "Failed to open %s: %s", bookPath, xtch::errorName(book.lastError()));
    loaded = false;
    requestUpdate();
    return;
  }
  loaded = true;
  loadProgress();
  if (page >= book.pageCount()) {
    LOG_INF("RDR", "Saved page %lu past end (%u), clamping", static_cast<unsigned long>(page), book.pageCount());
    page = book.pageCount() > 0 ? book.pageCount() - 1 : 0;
  }
  snprintf(settings.lastBookPath, sizeof(settings.lastBookPath), "%s", bookPath);
  settings.save();
  LOG_INF("RDR", "Open %s page %lu/%u '%s'", bookPath, static_cast<unsigned long>(page + 1), book.pageCount(),
          book.title());
  requestUpdate();
}

void ReaderScreen::onExit() {
  if (loaded) {
    saveProgress();
  }
  book.close();
  halTiltSensor.clearPendingEvents();
  Screen::onExit();
}

void ReaderScreen::onResume() { pagesUntilFull = 1; }

void ReaderScreen::loop() {
  if (!loaded) {
    if (input.wasReleased(MappedInput::Button::Back) || input.wasReleased(MappedInput::Button::Confirm)) {
      finish();
    }
    return;
  }

  // Opportunistic: finish any deferred post-page-render RAM cleanup here,
  // before checking input, so it lands in idle time rather than the next
  // page's blocking render.
  book.flushPendingCleanup(gfx);

  if (input.wasReleased(MappedInput::Button::Back)) {
    finish();
    return;
  }
  if (input.wasReleased(MappedInput::Button::Confirm)) {
    pagesUntilFull = 1;
    auto screen = makeUniqueNoThrow<ChapterSelectionScreen>(gfx, input, *this, book.getChapters(), page,
                                                            book.pageCount());
    if (!screen) {
      LOG_ERR("RDR", "OOM: chapters");
      return;
    }
    push(std::move(screen));
    return;
  }

  bool moved = false;
  const int delta = input.consumeReaderPageDelta();

  if (delta > 0) {
    const uint32_t maxForward = book.pageCount() > 0 ? book.pageCount() - 1 - page : 0;
    const uint32_t step = std::min(static_cast<uint32_t>(delta), maxForward);
    if (step > 0) {
      page += step;
      moved = true;
    } else {
      LOG_DBG("RDR", "Already last page");
    }
  } else if (delta < 0) {
    const uint32_t step = std::min(static_cast<uint32_t>(-delta), page);
    if (step > 0) {
      page -= step;
      moved = true;
    } else {
      LOG_DBG("RDR", "Already first page");
    }
  }
  if (moved) {
    LOG_DBG("RDR", "Page %lu/%u", static_cast<unsigned long>(page + 1), book.pageCount());
    saveProgress();
    requestUpdate();
  }
}

void ReaderScreen::showStatus(const char* title, const char* detail) {
  gfx.clear(false);
  gfx.drawCenteredText(FONT_UI_BOLD, gfx.height() / 2 - 20, title);
  if (detail && detail[0] != '\0') {
    gfx.drawCenteredText(FONT_UI, gfx.height() / 2 + 10, detail);
  }
  gfx.present(HalDisplay::HALF_REFRESH);
}

void ReaderScreen::jumpToPage(const uint32_t targetPage) {
  if (!loaded || targetPage >= book.pageCount()) {
    return;
  }
  page = targetPage;
  pagesUntilFull = 1;
  LOG_INF("RDR", "Jumped to page %lu/%u", static_cast<unsigned long>(page + 1), book.pageCount());
  saveProgress();
  requestUpdate();
}

void ReaderScreen::render() {
  if (!loaded) {
    showStatus("Could not open book", xtch::errorName(book.lastError()));
    return;
  }

  const unsigned long blitStart = millis();
  if (!book.drawPage(gfx, page, pagesUntilFull, settings.refreshEveryNPages)) {
    LOG_ERR("RDR", "Blit page %lu failed: %s", static_cast<unsigned long>(page), xtch::errorName(book.lastError()));
    showStatus(xtch::errorName(book.lastError()));
    return;
  }
  const unsigned long blitMs = millis() - blitStart;
  LOG_DBG("RDR", "Blit page %lu/%u %lums", static_cast<unsigned long>(page + 1), book.pageCount(), blitMs);
  if (power::tiltLocked()) {
    power::paintGyroOffMarker();
  }

  // Skip prefetch when the user already queued a skip-ahead or back-turn
  // during the blit; loading N+1 would delay that jump by ~146 ms.
  const int queued = input.queuedPageDelta();
  if (queued >= 0 && queued <= 1) {
    book.prefetchForward(page);
  } else {
    LOG_DBG("RDR", "Skip prefetch (queued delta %d)", queued);
  }
}

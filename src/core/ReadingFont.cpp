#include "core/ReadingFont.h"

#include <HalStorage.h>
#include <Logging.h>
#include <XgfFont.h>

#include <cstdio>
#include <cstring>

#include "core/Settings.h"

namespace {

bool endsWithI(const char* name, const char* ext) {
  const size_t n = strlen(name);
  const size_t e = strlen(ext);
  if (n < e) {
    return false;
  }
  for (size_t i = 0; i < e; ++i) {
    char a = name[n - e + i];
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<char>(a - 'A' + 'a');
    }
    if (a != ext[i]) {
      return false;
    }
  }
  return true;
}

bool illegalFatChar(const char c) {
  return c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
         c == '|';
}

bool firstFilename(char* out, const size_t outSize) {
  if (!out || outSize < 6) {
    return false;
  }
  out[0] = '\0';
  HalFile dir = Storage.open(ReadingFont::kDir);
  if (!dir || !dir.isDirectory()) {
    return false;
  }
  char name[HalFile::kMaxNameBytes];
  for (HalFile file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory() || file.getName(name, sizeof(name)) == 0) {
      continue;
    }
    if (!ReadingFont::isFontFilename(name)) {
      continue;
    }
    snprintf(out, outSize, "%s", name);
    return true;
  }
  return false;
}

}  // namespace

void ReadingFont::migrate() {
  Storage.ensureDirectoryExists(Settings::kDir);
  Storage.ensureDirectoryExists(kDir);
  if (!Storage.exists(kLegacyPath)) {
    return;
  }
  char dest[192];
  makePath(dest, sizeof(dest), "reading.xgf2");
  if (!Storage.exists(dest)) {
    if (!Storage.rename(kLegacyPath, dest)) {
      LOG_ERR("FONT", "Could not move %s -> %s", kLegacyPath, dest);
      return;
    }
    LOG_INF("FONT", "Moved legacy font to %s", dest);
  } else if (!Storage.remove(kLegacyPath)) {
    LOG_ERR("FONT", "Could not remove leftover %s", kLegacyPath);
    return;
  }
  if (!settings.fontFile[0]) {
    setActive("reading.xgf2");
  }
}

bool ReadingFont::isFontFilename(const char* name) {
  if (!name || name[0] == '\0' || name[0] == '.') {
    return false;
  }
  const size_t n = strlen(name);
  if (n > kMaxFileName || !endsWithI(name, ".xgf2")) {
    return false;
  }
  for (size_t i = 0; i < n; ++i) {
    if (illegalFatChar(name[i])) {
      return false;
    }
  }
  return true;
}

bool ReadingFont::copyFilename(char* out, const size_t outSize, const char* name) {
  if (!out || outSize == 0) {
    return false;
  }
  out[0] = '\0';
  if (!name) {
    return false;
  }
  const char* slash = strrchr(name, '/');
  const char* base = slash ? slash + 1 : name;
  if (!isFontFilename(base) || strlen(base) >= outSize) {
    return false;
  }
  snprintf(out, outSize, "%s", base);
  return true;
}

void ReadingFont::makePath(char* out, const size_t outSize, const char* filename) {
  snprintf(out, outSize, "%s/%s", kDir, filename ? filename : "");
}

bool ReadingFont::activePath(char* out, const size_t outSize) {
  if (!out || outSize == 0) {
    return false;
  }
  out[0] = '\0';
  if (settings.fontFile[0]) {
    makePath(out, outSize, settings.fontFile);
    if (Storage.exists(out)) {
      return true;
    }
  }
  char name[kMaxFileName + 1];
  if (firstFilename(name, sizeof(name))) {
    makePath(out, outSize, name);
    return true;
  }
  if (Storage.exists(kLegacyPath)) {
    snprintf(out, outSize, "%s", kLegacyPath);
    return true;
  }
  out[0] = '\0';
  return false;
}

bool ReadingFont::setActive(const char* filename) {
  char name[kMaxFileName + 1];
  if (!copyFilename(name, sizeof(name), filename)) {
    return false;
  }
  char path[192];
  makePath(path, sizeof(path), name);
  if (!Storage.exists(path)) {
    LOG_ERR("FONT", "Missing %s", path);
    return false;
  }
  snprintf(settings.fontFile, sizeof(settings.fontFile), "%s", name);
  settings.save();
  LOG_INF("FONT", "Active %s", name);
  return true;
}

bool ReadingFont::loadUi(XgfFont& font) {
  char path[192];
  if (!activePath(path, sizeof(path))) {
    return false;
  }
  return font.load(path, XgfFont::kUiLruBytes, false);
}

#include "network/FileTransferServer.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WebServer.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>

#include "core/Settings.h"
#include "network/html/FileManagerPage.h"

#ifndef CROSSXTCH_VERSION
#define CROSSXTCH_VERSION "dev"
#endif

namespace {
// Appends `s` to `out`, escaping '"' and '\' so it stays valid inside a JSON string.
void appendJsonEscaped(std::string& out, const char* s) {
  for (const char* p = s; *p; ++p) {
    if (*p == '"' || *p == '\\') {
      out.push_back('\\');
    }
    out.push_back(*p);
  }
}

std::string joinPath(const char* dir, const char* name) {
  if (!dir || dir[0] == '\0' || (dir[0] == '/' && dir[1] == '\0')) {
    std::string out = "/";
    out += name ? name : "";
    return out;
  }
  std::string out = dir;
  if (out.back() != '/') {
    out += '/';
  }
  out += name ? name : "";
  return out;
}

// Directory portion of `path` (parent folder). "/" if path has no parent.
std::string dirnameOf(const char* path) {
  const char* slash = strrchr(path, '/');
  if (!slash || slash == path) {
    return "/";
  }
  return std::string(path, static_cast<size_t>(slash - path));
}

// Final path segment (file/folder name) of `path`.
const char* basenameOf(const char* path) {
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

bool isProtectedPath(const char* path) {
  const char* name = basenameOf(path);
  return name[0] == '.' || strcmp(name, "System Volume Information") == 0;
}

// WebServer::handleClient() sets a 5s socket timeout before parsing. That's
// too tight for a book upload: a Wi-Fi hiccup or a blocking SD write on the
// single-core C3 makes client.readBytes() return 0, the raw parser aborts,
// and Chrome logs net::ERR_CONNECTION_RESET. 60s is per-chunk, not overall.
constexpr uint32_t kUploadSocketTimeoutMs = 60000;

void pumpNetwork() {
  feedLoopWDT();
  yield();
}
}  // namespace

class FileTransferServer::RawUploadHandler : public RequestHandler {
  FileTransferServer& owner;

 public:
  explicit RawUploadHandler(FileTransferServer& owner) : owner(owner) {}

  bool canHandle(WebServer& /*server*/, HTTPMethod method, const String& uri) override {
    return method == HTTP_POST && uri == "/upload";
  }
  bool canRaw(WebServer& /*server*/, const String& uri) override { return uri == "/upload"; }

  void raw(WebServer& /*server*/, const String& /*uri*/, HTTPRaw& raw) override {
    switch (raw.status) {
      case RAW_START:
        owner.handleUploadStart();
        break;
      case RAW_WRITE:
        owner.handleUploadChunk(raw.buf, raw.currentSize);
        break;
      case RAW_END:
        owner.handleUploadEnd(raw.totalSize);
        break;
      case RAW_ABORTED:
        owner.handleUploadAbort();
        break;
    }
  }

  bool handle(WebServer& /*server*/, HTTPMethod /*method*/, const String& /*uri*/) override {
    owner.sendUploadResponse();
    return true;
  }
};

FileTransferServer::FileTransferServer() = default;
FileTransferServer::~FileTransferServer() { stop(); }

bool FileTransferServer::begin() {
  server = makeUniqueNoThrow<WebServer>(80);
  if (!server) {
    LOG_ERR("XFER", "OOM: WebServer");
    return false;
  }

  // Query-string args aren't parsed for raw-body requests; path/name travel
  // as headers. Body size is WebServer::clientContentLength().
  static const char* kCollectedHeaders[] = {"X-File-Path", "X-File-Name"};
  server->collectHeaders(kCollectedHeaders, 2);

  uploadHandler = new (std::nothrow) RawUploadHandler(*this);
  if (!uploadHandler) {
    LOG_ERR("XFER", "OOM: upload handler");
    return false;
  }
  server->addHandler(uploadHandler);

  server->on("/", HTTP_GET, [this]() { handleRoot(); });
  server->on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  server->on("/api/timezone", HTTP_POST, [this]() { handleTimezone(); });
  server->on("/api/files", HTTP_GET, [this]() { handleFileList(); });
  server->on("/download", HTTP_GET, [this]() { handleDownload(); });
  server->on("/mkdir", HTTP_POST, [this]() { handleMkdir(); });
  server->on("/rename", HTTP_POST, [this]() { handleRename(); });
  server->on("/move", HTTP_POST, [this]() { handleMove(); });
  server->on("/delete", HTTP_POST, [this]() { handleDelete(); });
  server->onNotFound([this]() { handleNotFound(); });

  server->begin();
  running = true;

  upload.writeBuffer = makeUniqueNoThrow<uint8_t[]>(UploadState::kWriteBufferSize);
  upload.writeBufferPos = 0;

  mdnsHostname = gpio.deviceIsX3() ? "x3" : "x4";
  mdnsStarted = MDNS.begin(mdnsHostname.c_str());
  if (mdnsStarted) {
    MDNS.addService("http", "tcp", 80);
    LOG_INF("XFER", "Server started on port 80, mdns=%s.local freeHeap=%u maxAlloc=%u", mdnsHostname.c_str(),
            static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
  } else {
    LOG_ERR("XFER", "mDNS failed to start");
    mdnsHostname.clear();
    LOG_INF("XFER", "Server started on port 80, freeHeap=%u maxAlloc=%u",
            static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));
  }
  return true;
}

void FileTransferServer::stop() {
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
    mdnsHostname.clear();
  }
  if (server) {
    // ~WebServer() deletes every handler registered via addHandler(),
    // including uploadHandler, so just drop our (non-owning) pointer.
    server->stop();
    server.reset();
  }
  uploadHandler = nullptr;
  running = false;
  LOG_INF("XFER", "Server stopped");
}

void FileTransferServer::handleClient() {
  if (server) {
    server->handleClient();
  }
}

void FileTransferServer::handleRoot() const { server->send_P(200, "text/html", FILE_MANAGER_PAGE); }

void FileTransferServer::handleStatus() const {
  char json[256];
  snprintf(json, sizeof(json),
           "{\"version\":\"" CROSSXTCH_VERSION
           "\",\"ip\":\"%s\",\"ssid\":\"%s\",\"freeHeap\":%lu,\"uptime\":%lu,\"utcOffsetQ\":%u}",
           WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), static_cast<unsigned long>(ESP.getFreeHeap()),
           static_cast<unsigned long>(millis() / 1000), settings.clockUtcOffsetQ);
  server->send(200, "application/json", json);
}

void FileTransferServer::handleTimezone() {
  if (!server->hasArg("offsetQ")) {
    server->send(400, "text/plain", "Missing offsetQ");
    return;
  }
  const int q = atoi(server->arg("offsetQ").c_str());
  if (q < 0 || q > 104) {
    server->send(400, "text/plain", "offsetQ must be 0–104 (UTC−12 to UTC+14, 15 min steps)");
    return;
  }
  settings.clockUtcOffsetQ = static_cast<uint8_t>(q);
  // Manual choice wins over the one-shot HTTP timezone lookup.
  if (!settings.clockHasBeenSynced) {
    settings.clockHasBeenSynced = 1;
  }
  settings.save();
  LOG_INF("XFER", "Timezone set to q=%d", q);
  server->send(200, "text/plain", "OK");
}

void FileTransferServer::handleFileList() const {
  std::string path = server->hasArg("path") ? server->arg("path").c_str() : "/";
  if (path.empty()) {
    path = "/";
  }

  HalFile dir = Storage.open(path.c_str());
  if (!dir || !dir.isDirectory()) {
    server->send(404, "application/json", "[]");
    return;
  }

  std::string json;
  json.reserve(1024);
  json.push_back('[');
  bool first = true;
  char name[HalFile::kMaxNameBytes];
  for (HalFile file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.getName(name, sizeof(name)) == 0) {
      LOG_ERR("XFER", "Skipping file with unreadable name");
      continue;
    }
    if (isProtectedPath(name)) {
      continue;
    }
    if (!first) {
      json.push_back(',');
    }
    first = false;
    json += "{\"name\":\"";
    appendJsonEscaped(json, name);
    json += "\",\"size\":";
    json += std::to_string(file.isDirectory() ? 0 : file.fileSize());
    json += ",\"isDirectory\":";
    json += file.isDirectory() ? "true" : "false";
    json += "}";
  }
  json.push_back(']');
  server->send(200, "application/json", json.c_str());
}

void FileTransferServer::handleDownload() const {
  if (!server->hasArg("path")) {
    server->send(400, "text/plain", "Missing path");
    return;
  }
  const std::string path = server->arg("path").c_str();
  if (isProtectedPath(path.c_str())) {
    server->send(403, "text/plain", "Protected file");
    return;
  }

  HalFile file;
  if (!Storage.openFileForRead("XFER", path.c_str(), file)) {
    server->send(404, "text/plain", "File not found");
    return;
  }

  std::string header = "attachment; filename=\"";
  header += basenameOf(path.c_str());
  header += '"';
  server->sendHeader("Content-Disposition", header.c_str());
  server->setContentLength(file.fileSize());
  server->send(200, "application/octet-stream", "");
  server->client().setNoDelay(true);

  constexpr size_t kChunkSize = 16384;
  auto buffer = makeUniqueNoThrow<uint8_t[]>(kChunkSize);
  if (!buffer) {
    LOG_ERR("XFER", "OOM: download buffer");
    return;
  }
  int n;
  while ((n = file.read(buffer.get(), kChunkSize)) > 0) {
    server->client().write(buffer.get(), static_cast<size_t>(n));
  }
}

void FileTransferServer::writeUploadBytes(const uint8_t* data, size_t len) {
  if (!upload.file || !len) {
    return;
  }
  const size_t written = upload.file.write(data, len);
  feedLoopWDT();
  if (written != len) {
    LOG_ERR("XFER", "Short upload write (%u of %u)", static_cast<unsigned>(written), static_cast<unsigned>(len));
    upload.success = false;
  }
}

void FileTransferServer::flushWriteBuffer() {
  if (upload.writeBufferPos == 0 || !upload.writeBuffer) {
    return;
  }
  writeUploadBytes(upload.writeBuffer.get(), upload.writeBufferPos);
  upload.writeBufferPos = 0;
}

void FileTransferServer::handleUploadStart() {
  upload.success = false;
  upload.preallocatedSize = 0;
  upload.writeBufferPos = 0;
  server->client().setNoDelay(true);
  server->client().setTimeout(kUploadSocketTimeoutMs);

  const std::string dir = WebServer::urlDecode(server->header("X-File-Path")).c_str();
  const std::string name = WebServer::urlDecode(server->header("X-File-Name")).c_str();
  if (name.empty()) {
    LOG_ERR("XFER", "Upload missing X-File-Name header");
    return;
  }
  upload.destPath = joinPath(dir.empty() ? "/" : dir.c_str(), name.c_str());
  // FAT open/preAllocate can block; yield so lwIP can ACK bytes already in flight.
  pumpNetwork();
  if (!Storage.openFileForWrite("XFER", upload.destPath.c_str(), upload.file)) {
    LOG_ERR("XFER", "Failed to create %s", upload.destPath.c_str());
    return;
  }
  upload.success = true;

  const int contentLength = server->clientContentLength();
  if (contentLength > 0) {
    if (upload.file.preAllocate(static_cast<uint64_t>(contentLength))) {
      upload.preallocatedSize = static_cast<uint64_t>(contentLength);
      LOG_INF("XFER", "preAllocate %d bytes", contentLength);
    } else {
      LOG_ERR("XFER", "preAllocate %d failed", contentLength);
    }
  }
  pumpNetwork();
}

void FileTransferServer::handleUploadChunk(const uint8_t* data, size_t len) {
  if (!upload.file || !len) {
    return;
  }
  if (!upload.writeBuffer) {
    writeUploadBytes(data, len);
    return;
  }
  size_t offset = 0;
  while (offset < len) {
    const size_t space = UploadState::kWriteBufferSize - upload.writeBufferPos;
    const size_t chunk = std::min(space, len - offset);
    memcpy(upload.writeBuffer.get() + upload.writeBufferPos, data + offset, chunk);
    upload.writeBufferPos += chunk;
    offset += chunk;
    if (upload.writeBufferPos == UploadState::kWriteBufferSize) {
      flushWriteBuffer();
    }
  }
}

void FileTransferServer::handleUploadEnd(size_t totalBytes) {
  if (upload.file) {
    flushWriteBuffer();
    if (upload.preallocatedSize > totalBytes) {
      upload.file.truncate(totalBytes);
    }
    upload.file.flush();
    upload.file.close();
    upload.file = HalFile();
  }
  pumpNetwork();
  LOG_INF("XFER", "Uploaded %s (%lu bytes)", upload.destPath.c_str(), static_cast<unsigned long>(totalBytes));
}

void FileTransferServer::handleUploadAbort() {
  upload.success = false;
  upload.writeBufferPos = 0;
  upload.file = HalFile();  // close before removing the partial file
  if (!upload.destPath.empty()) {
    Storage.remove(upload.destPath.c_str());
  }
  LOG_ERR("XFER", "Upload aborted: %s", upload.destPath.c_str());
}

void FileTransferServer::sendUploadResponse() const {
  if (upload.success) {
    std::string msg = "File uploaded successfully: ";
    msg += basenameOf(upload.destPath.c_str());
    server->send(200, "text/plain", msg.c_str());
  } else {
    server->send(500, "text/plain", "Upload failed");
  }
}

void FileTransferServer::handleMkdir() const {
  if (!server->hasArg("name")) {
    server->send(400, "text/plain", "Missing name");
    return;
  }
  const std::string parent = server->hasArg("path") ? server->arg("path").c_str() : "/";
  const std::string name = server->arg("name").c_str();
  const std::string full = joinPath(parent.c_str(), name.c_str());
  if (Storage.mkdir(full.c_str())) {
    server->send(200, "text/plain", "OK");
  } else {
    server->send(500, "text/plain", "Could not create folder");
  }
}

void FileTransferServer::handleRename() const {
  if (!server->hasArg("path") || !server->hasArg("name")) {
    server->send(400, "text/plain", "Missing path or name");
    return;
  }
  const std::string oldPath = server->arg("path").c_str();
  const std::string newName = server->arg("name").c_str();
  if (isProtectedPath(oldPath.c_str())) {
    server->send(403, "text/plain", "Protected file");
    return;
  }
  const std::string dir = dirnameOf(oldPath.c_str());
  const std::string newPath = joinPath(dir.c_str(), newName.c_str());
  if (Storage.rename(oldPath.c_str(), newPath.c_str())) {
    server->send(200, "text/plain", "OK");
  } else {
    server->send(500, "text/plain", "Rename failed");
  }
}

void FileTransferServer::handleMove() const {
  if (!server->hasArg("path") || !server->hasArg("dest")) {
    server->send(400, "text/plain", "Missing path or dest");
    return;
  }
  const std::string oldPath = server->arg("path").c_str();
  const std::string dest = server->arg("dest").c_str();
  if (isProtectedPath(oldPath.c_str())) {
    server->send(403, "text/plain", "Protected file");
    return;
  }
  const std::string newPath = joinPath(dest.c_str(), basenameOf(oldPath.c_str()));
  if (Storage.rename(oldPath.c_str(), newPath.c_str())) {
    server->send(200, "text/plain", "OK");
  } else {
    server->send(500, "text/plain", "Move failed");
  }
}

void FileTransferServer::handleDelete() const {
  if (!server->hasArg("path")) {
    server->send(400, "text/plain", "Missing path");
    return;
  }
  const std::string path = server->arg("path").c_str();
  if (isProtectedPath(path.c_str())) {
    server->send(403, "text/plain", "Protected file");
    return;
  }
  HalFile file = Storage.open(path.c_str());
  const bool isDir = file && file.isDirectory();
  file = HalFile();  // close before remove/rmdir
  const bool ok = isDir ? Storage.rmdir(path.c_str()) : Storage.remove(path.c_str());
  if (ok) {
    server->send(200, "text/plain", "OK");
  } else {
    server->send(500, "text/plain", isDir ? "Folder not empty or missing" : "Delete failed");
  }
}

void FileTransferServer::handleNotFound() const { server->send(404, "text/plain", "Not found"); }

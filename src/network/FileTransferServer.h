#pragma once

#include <HalStorage.h>

#include <memory>
#include <string>

class WebServer;

// HTTP-only (no WebSocket/WebDAV) file transfer server: serves a small static
// file manager page and a handful of JSON/file endpoints against the SD card.
class FileTransferServer {
 public:
  FileTransferServer();
  ~FileTransferServer();

  // Starts listening on port 80. Call once Wi-Fi is connected. Advertises
  // "x3.local"/"x4.local" via mDNS depending on the running device.
  bool begin();
  void stop();
  void handleClient();
  bool isRunning() const { return running; }
  // Empty if mDNS didn't start (e.g. begin() not called or it failed).
  const std::string& hostname() const { return mdnsHostname; }

 private:
  std::unique_ptr<WebServer> server;
  bool running = false;
  bool mdnsStarted = false;
  std::string mdnsHostname;

  // One upload at a time. HTTP_RAW_BUFLEN (4 KB) is coalesced into a 16 KB
  // SD write so we issue fewer SPI transactions per book.
  struct UploadState {
    HalFile file;
    std::string destPath;
    bool success = false;
    uint64_t preallocatedSize = 0;
    static constexpr size_t kWriteBufferSize = 16384;
    std::unique_ptr<uint8_t[]> writeBuffer;
    size_t writeBufferPos = 0;
  } upload;

  void handleRoot() const;
  void handleStatus() const;
  void handleTimezone();
  void handleFileList() const;
  void handleDownload() const;
  void handleMkdir() const;
  void handleRename() const;
  void handleMove() const;
  void handleDelete() const;
  void handleNotFound() const;

  // Raw (non-multipart) upload: registered via a custom RequestHandler so
  // WebServer bulk-reads the body straight into our buffer instead of
  // parsing it byte-by-byte through multipart boundary matching. Path and
  // filename travel as request headers (X-File-Path/X-File-Name) since the
  // query string isn't parsed for raw-body requests. See RawUploadHandler.
  //
  // Non-owning: WebServer::addHandler() takes ownership and deletes every
  // registered handler in ~WebServer(), so this must NOT also be deleted
  // here (that previously caused a double-free/heap corruption on stop()).
  class RawUploadHandler;
  RawUploadHandler* uploadHandler = nullptr;
  void handleUploadStart();
  void handleUploadChunk(const uint8_t* data, size_t len);
  void handleUploadEnd(size_t totalBytes);
  void handleUploadAbort();
  void sendUploadResponse() const;
  void writeUploadBytes(const uint8_t* data, size_t len);
  void flushWriteBuffer();
};

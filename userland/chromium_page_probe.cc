// Copyright 2026 Zircon Embedded contributors.
// Use of this source code is governed by the BSD license in LICENSE.
// Observe the real browser document through its loopback DevTools endpoint.
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "third_party/rapidjson/include/rapidjson/document.h"
#include "third_party/rapidjson/include/rapidjson/stringbuffer.h"
#include "third_party/rapidjson/include/rapidjson/writer.h"

namespace {
constexpr size_t kLimit = 1024 * 1024;
int Fail(const char* message) {
  std::fprintf(stderr, "[chromium_page_probe] %s\n", message);
  return 1;
}
bool Transfer(int fd, void* data, size_t size, bool writing) {
  auto* bytes = static_cast<uint8_t*>(data);
  while (size) {
    ssize_t n = writing ? send(fd, bytes, size, MSG_NOSIGNAL) : recv(fd, bytes, size, 0);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    bytes += n;
    size -= static_cast<size_t>(n);
  }
  return true;
}
bool Send(int fd, const std::string& value) {
  return Transfer(fd, const_cast<char*>(value.data()), value.size(), true);
}
int Connect() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  // ARM64 runs under TCG; a busy renderer can take longer to service DevTools.
#if defined(__aarch64__)
  timeval timeout{30, 0};
#else
  timeval timeout{3, 0};
#endif
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(9222);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}
bool Header(int fd, std::string* header) {
  char byte;
  while (header->size() < 65536) {
    if (!Transfer(fd, &byte, 1, false)) return false;
    *header += byte;
    if (header->ends_with("\r\n\r\n")) return true;
  }
  return false;
}
bool YahooUrl(const char* url) {
  std::string s(url);
  return s.starts_with("https://www.yahoo.com/") || s.starts_with("https://yahoo.com/");
}
bool Target(std::string* path) {
  int fd = Connect();
  if (fd < 0) return false;
  std::string header;
  bool ok = Send(fd, "GET /json/list HTTP/1.1\r\nHost: 127.0.0.1:9222\r\nConnection: close\r\n\r\n") && Header(fd, &header);
  std::string body;
  if (ok && header.starts_with("HTTP/1.1 200 ")) {
    std::string lower = header;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto field = lower.find("\r\ncontent-length:");
    if (field == std::string::npos) {
      ok = false;
    } else {
      char* end;
      unsigned long length = std::strtoul(lower.c_str() + field + 17, &end, 10);
      if (end == lower.c_str() + field + 17 || *end != '\r' || length > kLimit) {
        ok = false;
      } else {
        body.resize(length);
        ok = Transfer(fd, body.data(), body.size(), false);
      }
    }
  } else {
    ok = false;
  }
  close(fd);
  rapidjson::Document list;
  if (!ok || list.Parse(body.c_str()).HasParseError() || !list.IsArray()) return false;
  for (const auto& entry : list.GetArray()) {
    if (!entry.IsObject() || !entry.HasMember("type") || !entry["type"].IsString() ||
        std::strcmp(entry["type"].GetString(), "page") || !entry.HasMember("url") ||
        !entry["url"].IsString() || !YahooUrl(entry["url"].GetString()) ||
        !entry.HasMember("webSocketDebuggerUrl") || !entry["webSocketDebuggerUrl"].IsString()) continue;
    std::string url(entry["webSocketDebuggerUrl"].GetString());
    auto slash = url.find('/', 5);  // Skip ws:// and its authority.
    if (!url.starts_with("ws://") || slash == std::string::npos) return false;
    *path = url.substr(slash);
    return path->starts_with("/devtools/page/") && path->find_first_of("\r\n ") == std::string::npos;
  }
  return false;
}
bool Frame(int fd, uint8_t opcode, const std::string& value) {
  if (value.size() > 65535) return false;
  // RFC 6455 requires a fresh mask for every client frame.
  std::array<uint8_t, 4> mask{};
  FILE* random = std::fopen("/dev/urandom", "rb");
  if (!random) return false;
  bool ok = std::fread(mask.data(), 1, mask.size(), random) == mask.size();
  std::fclose(random);
  if (!ok) return false;
  std::string frame;
  frame += static_cast<char>(0x80 | opcode);
  if (value.size() < 126) {
    frame += static_cast<char>(0x80 | value.size());
  } else {
    frame += static_cast<char>(0x80 | 126);
    frame += static_cast<char>(value.size() >> 8);
    frame += static_cast<char>(value.size());
  }
  frame.append(reinterpret_cast<const char*>(mask.data()), mask.size());
  for (size_t i = 0; i < value.size(); ++i) frame += static_cast<char>(value[i] ^ mask[i % 4]);
  return Send(fd, frame);
}
bool Message(int fd, std::string* message) {
  bool started = false;
  for (unsigned frames = 0; frames < 128; ++frames) {
    uint8_t header[2];
    if (!Transfer(fd, header, 2, false) || (header[0] & 0x70) || (header[1] & 0x80)) return false;
    uint8_t opcode = header[0] & 15;
    uint64_t length = header[1] & 127;
    if (length == 126 || length == 127) {
      unsigned bytes = length == 126 ? 2 : 8;
      length = 0;
      for (unsigned i = 0; i < bytes; ++i) {
        uint8_t byte;
        if (!Transfer(fd, &byte, 1, false)) return false;
        length = (length << 8) | byte;
      }
    }
    if (length > kLimit || message->size() + length > kLimit) return false;
    std::string payload(static_cast<size_t>(length), '\0');
    if (!Transfer(fd, payload.data(), payload.size(), false)) return false;
    if (opcode == 8) return false;
    if (opcode == 9) { if (!Frame(fd, 10, payload)) return false; continue; }
    if (opcode == 10) continue;
    if ((opcode != 1 && opcode != 0) || (opcode == 0 && !started) || (opcode == 1 && started)) return false;
    started = true;
    *message += payload;
    if (header[0] & 0x80) return true;
  }
  return false;
}
bool Text(const rapidjson::Value& value, const char* key) {
  return value.HasMember(key) && value[key].IsString();
}
}  // namespace

int main(int argc, char** argv) {
  std::string nonce;
  uint32_t a = 0, b = 0;
  if (argc == 4) {
    nonce = argv[1];
    if (nonce.size() != 32 || nonce.find_first_not_of("0123456789abcdef") != std::string::npos) return Fail("invalid nonce");
    char *end_a, *end_b;
    unsigned long x = std::strtoul(argv[2], &end_a, 10), y = std::strtoul(argv[3], &end_b, 10);
    if (*end_a || *end_b || x < 2 || y < 2 || x > 65535 || y > 65535) return Fail("invalid factors");
    a = x; b = y;
  } else if (argc == 1) {
    std::array<uint8_t, 20> entropy{};
    FILE* random = std::fopen("/dev/urandom", "rb");
    if (!random) return Fail("cannot open entropy source");
    bool ok = std::fread(entropy.data(), 1, entropy.size(), random) == entropy.size();
    std::fclose(random);
    if (!ok) return Fail("cannot read entropy source");
    for (size_t i = 0; i < 16; ++i) {
      nonce += "0123456789abcdef"[entropy[i] >> 4];
      nonce += "0123456789abcdef"[entropy[i] & 15];
    }
    a = 2 + ((entropy[16] << 8 | entropy[17]) % 60000);
    b = 2 + ((entropy[18] << 8 | entropy[19]) % 60000);
  } else {
    return Fail("usage: chromium_page_probe [32-hex-nonce factor factor]");
  }
  std::string path;
  if (!Target(&path)) return Fail("no Yahoo page target on DevTools port 9222");
  int fd = Connect();
  if (fd < 0) return Fail("cannot connect to DevTools");
  std::string header;
  std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1:9222\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
  if (!Send(fd, request) || !Header(fd, &header) || !header.starts_with("HTTP/1.1 101 ") ||
      header.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == std::string::npos) {
    close(fd); return Fail("invalid WebSocket upgrade response");
  }
  std::string expression = "({nonce:'" + nonce + "',answer:(" + std::to_string(a) + "*" + std::to_string(b) +
      "),href:location.href,uri:document.documentURI,title:document.title,ready:document.readyState,"
      "secure:isSecureContext,text_length:document.body?document.body.innerText.length:0,"
      "http_status:(performance.getEntriesByType('navigation')[0]||{}).responseStatus})";
  rapidjson::StringBuffer encoded;
  rapidjson::Writer<rapidjson::StringBuffer> writer(encoded);
  writer.StartObject(); writer.Key("id"); writer.Int(7); writer.Key("method"); writer.String("Runtime.evaluate");
  writer.Key("params"); writer.StartObject(); writer.Key("expression"); writer.String(expression.c_str());
  writer.Key("returnByValue"); writer.Bool(true); writer.EndObject(); writer.EndObject();
  if (!Frame(fd, 1, encoded.GetString())) { close(fd); return Fail("cannot send renderer challenge"); }
  for (unsigned count = 0; count < 32; ++count) {
    std::string message;
    errno = 0;
    if (!Message(fd, &message)) {
      std::fprintf(stderr, "[chromium_page_probe] WebSocket receive failed: errno=%d (%s)\n",
                   errno, std::strerror(errno));
      break;
    }
    rapidjson::Document reply;
    if (reply.Parse(message.c_str()).HasParseError() || !reply.IsObject()) break;
    if (!reply.HasMember("id")) continue;
    if (!reply["id"].IsInt() || reply["id"].GetInt() != 7) continue;
    close(fd);
    if (reply.HasMember("error") || !reply.HasMember("result") || !reply["result"].IsObject()) return Fail("DevTools rejected evaluation");
    const auto& result = reply["result"];
    if (result.HasMember("exceptionDetails") || !result.HasMember("result") || !result["result"].IsObject() ||
        !result["result"].HasMember("value") || !result["result"]["value"].IsObject()) return Fail("renderer did not return a document observation");
    const auto& value = result["result"]["value"];
    rapidjson::StringBuffer observed;
    rapidjson::Writer<rapidjson::StringBuffer> output(observed);
    value.Accept(output);
    if (!Text(value, "nonce") || nonce != value["nonce"].GetString() ||
        !value.HasMember("answer") || !value["answer"].IsNumber() || value["answer"].GetDouble() != static_cast<double>(uint64_t{a} * b) ||
        !Text(value, "href") || !YahooUrl(value["href"].GetString()) ||
        !Text(value, "uri") || !YahooUrl(value["uri"].GetString()) ||
        !Text(value, "ready") || std::strcmp(value["ready"].GetString(), "complete") ||
        !Text(value, "title") || std::strstr(value["title"].GetString(), "Yahoo") == nullptr ||
        !value.HasMember("secure") || !value["secure"].IsBool() || !value["secure"].GetBool() ||
        !value.HasMember("http_status") || !value["http_status"].IsInt() ||
        value["http_status"].GetInt() < 200 || value["http_status"].GetInt() >= 300 ||
        !value.HasMember("text_length") || !value["text_length"].IsUint64() || value["text_length"].GetUint64() < 200) {
      std::fprintf(stderr, "[chromium_page_probe] observed %s\n", observed.GetString());
      return Fail("document is not a loaded, secure Yahoo page or challenge failed");
    }
    std::printf("[CHROMIUM_PAGE] %s\n", observed.GetString());
    return 0;
  }
  close(fd);
  return Fail("renderer challenge timed out or protocol failed");
}

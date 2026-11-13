#include "WikipediaActivity.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

#include <PersistableStore.h>
#include <SdCardFontSystem.h>

#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/reader/ReaderActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/HttpDownloader.h"

namespace {
constexpr const char* kWikiDir = "/wikipedia";
constexpr size_t kMaxSearchBytes = 48 * 1024;
constexpr size_t kMaxParseBytes = 220 * 1024;  // bounded for RAM
constexpr size_t kMaxTextChars = 120 * 1024;
constexpr int kMaxImages = 20;

bool ensureDir(const char* path) {
  if (Storage.exists(path)) return true;
  return Storage.mkdir(path);
}

std::string trimCopy(const std::string& s) {
  size_t a = 0;
  while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  size_t b = s.size();
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

std::string toLower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool endsWithNoCase(const std::string& s, const char* suf) {
  const size_t n = std::strlen(suf);
  if (s.size() < n) return false;
  for (size_t i = 0; i < n; ++i) {
    char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[s.size() - n + i])));
    char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suf[i])));
    if (a != b) return false;
  }
  return true;
}

std::string htmlEscape(const std::string& in) {
  std::string out;
  out.reserve(in.size() + 16);
  for (unsigned char c : in) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += static_cast<char>(c); break;
    }
  }
  return out;
}

void appendUtf8(std::string& out, uint32_t cp) {
  if (cp < 0x80) out += static_cast<char>(cp);
  else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

std::string decodeHtmlEntities(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    if (in[i] != '&') { out += in[i]; continue; }
    size_t semi = in.find(';', i + 1);
    if (semi == std::string::npos || semi - i > 32) { out += in[i]; continue; }
    const std::string ent = in.substr(i + 1, semi - i - 1);
    bool handled = false;
    if (!ent.empty() && ent[0] == '#') {
      uint32_t cp = 0;
      bool ok = false;
      if (ent.size() >= 2 && (ent[1] == 'x' || ent[1] == 'X')) {
        ok = true;
        for (size_t k = 2; k < ent.size(); ++k) {
          char c = ent[k]; cp <<= 4;
          if (c >= '0' && c <= '9') cp |= (c - '0');
          else if (c >= 'a' && c <= 'f') cp |= (c - 'a' + 10);
          else if (c >= 'A' && c <= 'F') cp |= (c - 'A' + 10);
          else { ok = false; break; }
        }
      } else {
        ok = true;
        for (size_t k = 1; k < ent.size(); ++k) {
          char c = ent[k];
          if (c < '0' || c > '9') { ok = false; break; }
          cp = cp * 10 + (uint32_t)(c - '0');
        }
      }
      if (ok) { appendUtf8(out, cp); handled = true; }
    } else if (ent == "amp") { out += '&'; handled = true; }
    else if (ent == "lt") { out += '<'; handled = true; }
    else if (ent == "gt") { out += '>'; handled = true; }
    else if (ent == "quot") { out += '"'; handled = true; }
    else if (ent == "apos" || ent == "#39") { out += '\''; handled = true; }
    else if (ent == "nbsp") { out += ' '; handled = true; }
    else if (ent == "ndash" || ent == "mdash") { out += '-'; handled = true; }
    else if (ent == "lsquo" || ent == "rsquo") { out += '\''; handled = true; }
    else if (ent == "ldquo" || ent == "rdquo") { out += '"'; handled = true; }
    else if (ent == "hellip") { out += "..."; handled = true; }
    if (handled) { i = semi; continue; }
    out += in[i];
  }
  return out;
}

bool extractJsonStringField(const std::string& src, const char* field, std::string& out) {
  const std::string key = std::string("\"") + field + "\"";
  size_t pos = src.find(key);
  if (pos == std::string::npos) return false;
  pos = src.find(':', pos + key.size());
  if (pos == std::string::npos) return false;
  ++pos;
  while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) ++pos;
  if (pos >= src.size() || src[pos] != '"') return false;
  ++pos;
  out.clear();
  while (pos < src.size()) {
    const char c = src[pos++];
    if (c == '"') return true;
    if (c == '\\' && pos < src.size()) {
      const char e = src[pos++];
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': break;
        case 'f': break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          if (pos + 4 <= src.size()) {
            unsigned int cp = 0;
            bool okhex = true;
            for (int i = 0; i < 4; ++i) {
              char h = src[pos + i];
              cp <<= 4;
              if (h >= '0' && h <= '9') cp |= (h - '0');
              else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
              else { okhex = false; break; }
            }
            if (okhex) {
              pos += 4;
              if (cp < 0x80) out += static_cast<char>(cp);
              else if (cp == 0x00A0) out += ' ';
              // drop other non-ascii for simplicity
            }
          }
          break;
        }
        default: out += e; break;
      }
    } else {
      out += c;
    }
  }
  return !out.empty();
}

// Minimal ZIP (store, no compression) writer for EPUB
struct ZipEntryInfo { std::string name; uint32_t crc; uint32_t size; uint32_t offset; };

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
  }
  return ~crc;
}

void writeLe16(HalFile& f, uint16_t v) { uint8_t b[2] = {(uint8_t)(v & 0xFF), (uint8_t)(v >> 8)}; f.write(b, 2); }
void writeLe32(HalFile& f, uint32_t v) { uint8_t b[4] = {(uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF), (uint8_t)((v >> 16) & 0xFF), (uint8_t)(v >> 24)}; f.write(b, 4); }

bool writeStoredEntry(HalFile& f, const std::string& name, const uint8_t* data, size_t size, std::vector<ZipEntryInfo>& entries) {
  ZipEntryInfo info;
  info.name = name;
  info.size = static_cast<uint32_t>(size);
  info.crc = crc32Update(0, data, size);
  info.offset = static_cast<uint32_t>(f.position());
  writeLe32(f, 0x04034b50u);
  writeLe16(f, 20); // ver needed
  writeLe16(f, 0);  // flags
  writeLe16(f, 0);  // method store
  writeLe16(f, 0); writeLe16(f, 0); // time/date
  writeLe32(f, info.crc);
  writeLe32(f, info.size);
  writeLe32(f, info.size);
  writeLe16(f, static_cast<uint16_t>(name.size()));
  writeLe16(f, 0); // extra
  if (!name.empty()) f.write(reinterpret_cast<const uint8_t*>(name.data()), name.size());
  if (size > 0 && f.write(data, size) != size) return false;
  entries.push_back(info);
  return true;
}

bool writeStoredString(HalFile& f, const std::string& name, const std::string& content, std::vector<ZipEntryInfo>& entries) {
  return writeStoredEntry(f, name, reinterpret_cast<const uint8_t*>(content.data()), content.size(), entries);
}

bool finishZip(HalFile& f, std::vector<ZipEntryInfo>& entries) {
  const uint32_t cdOffset = static_cast<uint32_t>(f.position());
  for (const auto& e : entries) {
    writeLe32(f, 0x02014b50u);
    writeLe16(f, 20); writeLe16(f, 20);
    writeLe16(f, 0); // flags
    writeLe16(f, 0); // store
    writeLe16(f, 0); writeLe16(f, 0);
    writeLe32(f, e.crc);
    writeLe32(f, e.size);
    writeLe32(f, e.size);
    writeLe16(f, static_cast<uint16_t>(e.name.size()));
    writeLe16(f, 0); writeLe16(f, 0); writeLe16(f, 0); writeLe16(f, 0);
    writeLe32(f, 0);
    writeLe32(f, e.offset);
    if (!e.name.empty()) f.write(reinterpret_cast<const uint8_t*>(e.name.data()), e.name.size());
  }
  const uint32_t cdSize = static_cast<uint32_t>(f.position()) - cdOffset;
  writeLe32(f, 0x06054b50u);
  writeLe16(f, 0); writeLe16(f, 0);
  writeLe16(f, static_cast<uint16_t>(entries.size()));
  writeLe16(f, static_cast<uint16_t>(entries.size()));
  writeLe32(f, cdSize);
  writeLe32(f, cdOffset);
  writeLe16(f, 0);
  f.flush();
  return true;
}

bool readFileBytes(const std::string& path, std::vector<uint8_t>& out) {
  HalFile file;
  if (!Storage.openFileForRead("WIKI", path.c_str(), file) || file.isDirectory()) return false;
  const size_t sz = file.fileSize();
  if (sz == 0 || sz > 512 * 1024) return false;
  out.resize(sz);
  return file.read(out.data(), sz) == static_cast<int>(sz);
}

std::string imageNameFor(const std::string& src, int idx) {
  std::string ext = ".jpg";
  std::string lower = toLower(src);
  if (lower.find(".png") != std::string::npos) ext = ".png";
  else if (lower.find(".jpg") != std::string::npos || lower.find(".jpeg") != std::string::npos) ext = ".jpg";
  else if (lower.find(".gif") != std::string::npos) ext = ".gif";
  else if (lower.find(".webp") != std::string::npos) ext = ".webp";
  char buf[32];
  snprintf(buf, sizeof(buf), "img_%02d%s", idx, ext.c_str());
  return buf;
}

std::string buildXhtmlFromParseHtml(const std::string& title, const std::string& html,
                                    const std::vector<std::pair<std::string, std::string>>& srcToLocal) {
  auto mapSrc = [&](const std::string& src) -> std::string {
    for (const auto& kv : srcToLocal) if (kv.first == src) return kv.second;
    return std::string();
  };
  std::string body;
  body.reserve(html.size() / 2 + 1024);
  body += "<h1>" + htmlEscape(title) + "</h1>\n";
  bool inTag = false;
  bool skipContent = false;
  std::string tag;
  std::string textBuf;
  size_t i = 0;
  const size_t N = html.size();
  auto flushText = [&]() {
    if (!textBuf.empty()) {
      if (!skipContent) {
        // collapse excessive whitespace
        std::string t;
        t.reserve(textBuf.size());
        bool ws = false;
        for (char c : textBuf) {
          unsigned char uc = (unsigned char)c;
          if (uc == '\n' || uc == '\r' || uc == '\t' || uc == ' ') { if (!ws) { t += ' '; ws = true; } }
          else { t += c; ws = false; }
        }
        t = decodeHtmlEntities(t);
        if (!t.empty() && !(t.size() == 1 && t[0] == ' ')) body += htmlEscape(t);
      }
      textBuf.clear();
    }
  };

  while (i < N) {
    char c = html[i];
    if (!inTag) {
      if (c == '<') { flushText(); inTag = true; tag.clear(); ++i; }
      else { textBuf += c; ++i; }
      continue;
    }
    if (c == '>') {
      inTag = false; ++i;
      std::string lt = tag;
      while (!lt.empty() && std::isspace((unsigned char)lt.front())) lt.erase(lt.begin());
      while (!lt.empty() && std::isspace((unsigned char)lt.back())) lt.pop_back();
      bool closing = !lt.empty() && lt[0] == '/';
      std::string name;
      size_t p = closing ? 1 : 0;
      while (p < lt.size() && !std::isspace((unsigned char)lt[p]) && lt[p] != '/') { name += (char)std::tolower((unsigned char)lt[p]); ++p; }
      bool selfClose = (!lt.empty() && lt.back() == '/') || name == "img" || name == "br" || name == "hr";

      if (name == "script" || name == "style") { skipContent = !closing; continue; }
      if (skipContent) continue;

      if (!closing) {
        if (name == "p") body += "<p>";
        else if (name == "h1" || name == "h2" || name == "h3" || name == "h4") body += "<" + name + ">";
        else if (name == "li") body += "<p>• ";
        else if (name == "br") body += "<br/>";
        else if (name == "img") {
          // extract src
          std::string src;
          auto spos = lt.find("src");
          if (spos != std::string::npos) {
            auto q = lt.find_first_of("\"'", spos);
            if (q != std::string::npos) {
              char qc = lt[q];
              auto e = lt.find(qc, q + 1);
              if (e != std::string::npos) src = lt.substr(q + 1, e - q - 1);
            }
          }
          if (!src.empty()) {
            src = decodeHtmlEntities(src);
            if (src.rfind("//", 0) == 0) src = "https:" + src;
            std::string local = mapSrc(src);
            if (local.empty()) local = mapSrc("//" + src.substr(src.rfind("://") == std::string::npos ? 0 : src.find("://") + 3));
            if (!local.empty()) {
              body += "<p><img src=\"" + local + "\" alt=\"image\"/></p>";
            }
          }
        }
      } else {
        if (name == "p" || name == "li") body += "</p>\n";
        else if (name == "h1" || name == "h2" || name == "h3" || name == "h4") body += "</" + name + ">\n";
      }
      continue;
    }
    tag += c; ++i;
  }
  flushText();
  if (body.size() > kMaxTextChars) body.resize(kMaxTextChars);
  std::string x = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                  "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><meta charset=\"utf-8\"/>"
                  "<title>Wikipedia: " + htmlEscape(title) + "</title></head><body>\n" + body + "\n</body></html>";
  return x;
}

}  // namespace

WikipediaActivity::WikipediaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("Wikipedia", renderer, mappedInput) {}

void WikipediaActivity::onEnter() {
  Activity::onEnter();
  ensureDir("/.crosspoint");
  ensureDir(kWikiDir);
  promptSearch();
}

std::string WikipediaActivity::urlEncodeComponent(const std::string& s) const {
  std::string out;
  char buf[4];
  for (unsigned char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else if (c == ' ') {
      out += "%20";
    } else {
      snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

std::string WikipediaActivity::sanitizeFilename(const std::string& title) const {
  std::string out;
  out.reserve(title.size());
  for (unsigned char c : title) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
        c == '_') out += static_cast<char>(c);
    else out += '_';
  }
  auto t = trimCopy(out);
  if (t.empty()) t = "article";
  if (t.size() > 60) t.resize(60);
  return t;
}

void WikipediaActivity::showError(const std::string& message) {
  state = State::Error;
  errorMessage = message;
  needsRedraw = true;
  requestUpdate();
}

void WikipediaActivity::promptSearch() {
  if (WiFi.status() != WL_CONNECTED) {
    state = State::EnterQuery;
    wifiSelectionPending = true;
    auto wifi = std::make_unique<WifiSelectionActivity>(renderer, mappedInput);
    startActivityForResult(std::move(wifi), [this](const ActivityResult& result) {
      wifiSelectionPending = false;
      if (result.isCancelled || WiFi.status() != WL_CONNECTED) { finish(); return; }
      promptSearch();
    });
    return;
  }

  state = State::EnterQuery;
  needsRedraw = true;
  auto keyboard = std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, "Wikipedia", currentQuery, 80);
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) { finish(); return; }
    const auto q = trimCopy(std::get<KeyboardResult>(result.data).text);
    if (q.empty()) { finish(); return; }
    currentQuery = q;
    startSearch(q);
  });
}

void WikipediaActivity::startSearch(const std::string& query) {
  state = State::Searching;
  statusLine = "Searching...";
  needsRedraw = true;
  requestUpdate(true);
  resultTitles.clear(); resultDescs.clear(); selectedResult = 0;

  if (WiFi.status() != WL_CONNECTED) { showError("WiFi not connected"); return; }

  std::string body; body.reserve(4096); bool truncated = false;
  const std::string url = "https://en.wikipedia.org/w/api.php?action=opensearch&search=" + urlEncodeComponent(query) + "&limit=8&namespace=0&format=json";
  const bool ok = HttpDownloader::fetchUrl(url, [&](const uint8_t* data, size_t len) {
    if (body.size() + len > kMaxSearchBytes) { const size_t room = kMaxSearchBytes > body.size() ? (kMaxSearchBytes - body.size()) : 0; if (room) body.append((const char*)data, room); truncated = true; return false; }
    body.append((const char*)data, len); return true;
  }, "", "", std::vector<HttpDownloader::Header>{});

  if ((!ok && !truncated) || body.empty()) { showError("Search failed"); return; }

  JsonDocument doc;
  if (deserializeJson(doc, body) || !doc.is<JsonArray>()) { showError("Search parse error"); return; }
  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() < 2 || !arr[1].is<JsonArray>()) { showError("No results"); return; }
  JsonArray titles = arr[1].as<JsonArray>();
  JsonArray descs = (arr.size() >= 3 && arr[2].is<JsonArray>()) ? arr[2].as<JsonArray>() : JsonArray();
  for (size_t i = 0; i < titles.size(); ++i) {
    const char* t = titles[i] | ""; if (!t || !*t) continue;
    resultTitles.emplace_back(t);
    resultDescs.emplace_back((i < descs.size()) ? (descs[i] | "") : "");
  }
  if (resultTitles.empty()) { showError("No results"); return; }
  state = State::Results; needsRedraw = true; requestUpdate();
}


void applyPreferredWikipediaFont(const std::string& bookPath) {
  // Prefer Literata-FNaskh for mixed English/Persian/Arabic content when installed on SD.
  const char* preferred = "Literata-FNaskh";
  const auto* family = sdFontSystem.registry().findFamily(preferred);
  if (!family) return;

  char settingsPath[96];
  snprintf(settingsPath, sizeof(settingsPath), "/.crosspoint/book_settings/%zu.json",
           std::hash<std::string>{}(bookPath));
  if (Storage.exists(settingsPath)) return;  // don't override existing per-book choice

  JsonDocument doc;
  doc["sdFontFamilyName"] = preferred;
  if (PersistableStoreBase::writeDocToFile(settingsPath, doc)) {
    LOG_INF("WIKI", "Set preferred font '%s' for %s", preferred, bookPath.c_str());
  }
}

void WikipediaActivity::fetchArticle(int index) {
  if (index < 0 || index >= static_cast<int>(resultTitles.size())) return;
  if (WiFi.status() != WL_CONNECTED) { showError("WiFi not connected"); return; }
  state = State::Fetching; statusLine = "Fetching page..."; needsRedraw = true; requestUpdate(true);

  const std::string title = resultTitles[index];
  std::string body; body.reserve(16384); bool truncated = false;
  const std::string url = "https://en.wikipedia.org/w/api.php?action=parse&page=" + urlEncodeComponent(title) +
                          "&prop=text|displaytitle&format=json&formatversion=2&redirects=1";
  const bool ok = HttpDownloader::fetchUrl(url, [&](const uint8_t* data, size_t len) {
    if (body.size() + len > kMaxParseBytes) { const size_t room = kMaxParseBytes > body.size() ? (kMaxParseBytes - body.size()) : 0; if (room) body.append((const char*)data, room); truncated = true; return false; }
    body.append((const char*)data, len); return true;
  }, "", "", std::vector<HttpDownloader::Header>{});

  if ((!ok && !truncated) || body.empty()) { showError("Fetch failed"); return; }

  std::string html, pageTitleText;
  extractJsonStringField(body, "text", html);
  extractJsonStringField(body, "title", pageTitleText);
  if (pageTitleText.empty()) extractJsonStringField(body, "displaytitle", pageTitleText);
  const std::string finalTitle = pageTitleText.empty() ? title : pageTitleText;
  if (html.empty()) { showError("Article parse error"); return; }

  // collect image srcs from html (prefer content images over icons/UI assets)
  std::vector<std::string> imgSrcs;
  std::vector<std::string> fallbackSrcs;
  size_t pos = 0;
  while (imgSrcs.size() < (size_t)kMaxImages) {
    pos = html.find("<img", pos);
    if (pos == std::string::npos) break;
    size_t end = html.find('>', pos);
    if (end == std::string::npos) break;
    std::string tag = html.substr(pos, end - pos + 1);
    auto spos = tag.find("src");
    if (spos != std::string::npos) {
      auto q = tag.find_first_of("\"'", spos);
      if (q != std::string::npos) {
        char qc = tag[q]; auto e = tag.find(qc, q + 1);
        if (e != std::string::npos) {
          std::string src = decodeHtmlEntities(tag.substr(q + 1, e - q - 1));
          if (src.rfind("//", 0) == 0) src = "https:" + src;
          std::string lower = toLower(src);
          // Skip raw SVGs (renderer support varies), but allow raster thumbnails of SVGs (e.g. *.svg.png)
          const bool isSvg = (endsWithNoCase(src, ".svg") || lower.find(".svg?") != std::string::npos) &&
                             !(endsWithNoCase(src, ".png") || endsWithNoCase(src, ".jpg") || endsWithNoCase(src, ".jpeg") || endsWithNoCase(src, ".webp") || endsWithNoCase(src, ".gif"));
          const bool isHttp = src.rfind("http", 0) == 0;
          const bool obviousUi = lower.find("/static/") != std::string::npos ||
                                 lower.find("wikipedia/commons/thumb/") == std::string::npos && lower.find("upload.wikimedia.org") == std::string::npos ||
                                 lower.find("speaker") != std::string::npos ||
                                 lower.find("magnify-clip") != std::string::npos ||
                                 lower.find("question_book") != std::string::npos ||
                                 lower.find("ambox") != std::string::npos ||
                                 lower.find("edit") != std::string::npos ||
                                 lower.find("folder") != std::string::npos ||
                                 lower.find("portal-puzzle") != std::string::npos ||
                                 lower.find("commons-logo") != std::string::npos;
          const bool contentLike = lower.find("upload.wikimedia.org") != std::string::npos &&
                                   (lower.find("/thumb/") != std::string::npos || lower.find("/commons/") != std::string::npos ||
                                    lower.find("/wikipedia/") != std::string::npos);
          if (isHttp && !isSvg) {
            auto seen = [&](const std::vector<std::string>& v) { return std::find(v.begin(), v.end(), src) != v.end(); };
            if (!seen(imgSrcs) && !seen(fallbackSrcs)) {
              if (contentLike && !obviousUi) imgSrcs.push_back(src);
              else if (!obviousUi) fallbackSrcs.push_back(src);
            }
          }
        }
      }
    }
    pos = end + 1;
  }
  for (const auto& src : fallbackSrcs) {
    if ((int)imgSrcs.size() >= kMaxImages) break;
    if (std::find(imgSrcs.begin(), imgSrcs.end(), src) == imgSrcs.end()) imgSrcs.push_back(src);
  }

  std::vector<std::pair<std::string, std::string>> srcToLocal;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> imageFiles;
  int imgIdx = 1;
  for (const auto& src : imgSrcs) {
    statusLine = "Fetching image...";
    needsRedraw = true; requestUpdate(true);
    std::string tmpPath = std::string(kWikiDir) + "/_img_tmp";
    if (Storage.exists(tmpPath.c_str())) Storage.remove(tmpPath.c_str());
    auto err = HttpDownloader::downloadToFile(src, tmpPath, nullptr, nullptr, "", "", {}, false);
    if (err == HttpDownloader::OK) {
      std::vector<uint8_t> bytes;
      if (readFileBytes(tmpPath, bytes) && !bytes.empty()) {
        std::string local = "images/" + imageNameFor(src, imgIdx++);
        srcToLocal.push_back({src, local});
        // also map protocol-relative form if present in html
        if (src.rfind("https:", 0) == 0) srcToLocal.push_back({"//" + src.substr(6), local});
        imageFiles.push_back({local, std::move(bytes)});
      }
    }
    if (Storage.exists(tmpPath.c_str())) Storage.remove(tmpPath.c_str());
    if ((int)imageFiles.size() >= kMaxImages) break;
  }

  statusLine = "Building EPUB...";
  needsRedraw = true; requestUpdate(true);

  const std::string safe = sanitizeFilename(finalTitle);
  const std::string epubPath = std::string(kWikiDir) + "/wikipedia-" + safe + ".epub";
  if (Storage.exists(epubPath.c_str())) Storage.remove(epubPath.c_str());

  HalFile epub;
  if (!Storage.openFileForWrite("WIKI", epubPath.c_str(), epub)) { showError("Save failed"); return; }

  std::vector<ZipEntryInfo> entries;
  // mimetype must be first and stored
  const std::string mimetype = "application/epub+zip";
  if (!writeStoredString(epub, "mimetype", mimetype, entries)) { epub.close(); showError("Save failed"); return; }

  const std::string container = "<?xml version=\"1.0\"?>\n<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">\n"
                                "  <rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles>\n</container>";
  writeStoredString(epub, "META-INF/container.xml", container, entries);

  const std::string xhtml = buildXhtmlFromParseHtml(finalTitle, html, srcToLocal);
  writeStoredString(epub, "OEBPS/chapter.xhtml", xhtml, entries);

  std::string manifestItems = "    <item id=\"chap\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/>\n";
  std::string spineItems = "    <itemref idref=\"chap\"/>\n";
  int mi = 1;
  for (const auto& img : imageFiles) {
    std::string media = "image/jpeg";
    if (endsWithNoCase(img.first, ".png")) media = "image/png";
    else if (endsWithNoCase(img.first, ".gif")) media = "image/gif";
    else if (endsWithNoCase(img.first, ".webp")) media = "image/webp";
    char idbuf[16]; snprintf(idbuf, sizeof(idbuf), "img%d", mi++);
    manifestItems += "    <item id=\"" + std::string(idbuf) + "\" href=\"" + img.first + "\" media-type=\"" + media + "\"/>\n";
    writeStoredEntry(epub, "OEBPS/" + img.first, img.second.data(), img.second.size(), entries);
  }

  const std::string opf = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                          "<package xmlns=\"http://www.idpf.org/2007/opf\" unique-identifier=\"uid\" version=\"3.0\">\n"
                          "  <metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n"
                          "    <dc:identifier id=\"uid\">wikipedia:" + htmlEscape(safe) + "</dc:identifier>\n"
                          "    <dc:title>Wikipedia: " + htmlEscape(finalTitle) + "</dc:title>\n"
                          "    <dc:language>en</dc:language>\n"
                          "  </metadata>\n"
                          "  <manifest>\n" + manifestItems + "  </manifest>\n"
                          "  <spine>\n" + spineItems + "  </spine>\n"
                          "</package>";
  writeStoredString(epub, "OEBPS/content.opf", opf, entries);

  finishZip(epub, entries);
  epub.close();

  applyPreferredWikipediaFont(epubPath);

  state = State::Opening; needsRedraw = true; requestUpdate();
  auto reader = ReaderActivity::create(renderer, mappedInput, epubPath, false);
  if (!reader) { showError("Could not open reader"); return; }
  startActivityForResult(std::move(reader), [this](const ActivityResult&) { state = State::Results; needsRedraw = true; requestUpdate(); });
}

void WikipediaActivity::loop() {
  if (wifiSelectionPending) return;
  if (state == State::Error) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) || mappedInput.wasReleased(MappedInputManager::Button::Confirm)) finish();
    return;
  }
  if (state == State::Results) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) { finish(); return; }
    const int count = static_cast<int>(resultTitles.size());
    if (count > 0) {
      if (mappedInput.wasReleased(MappedInputManager::Button::Up) || mappedInput.wasReleased(MappedInputManager::Button::Left)) {
        selectedResult = (selectedResult + count - 1) % count; needsRedraw = true; requestUpdate(); return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Down) || mappedInput.wasReleased(MappedInputManager::Button::Right)) {
        selectedResult = (selectedResult + 1) % count; needsRedraw = true; requestUpdate(); return;
      }
      if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { fetchArticle(selectedResult); return; }
      int tx = 0, ty = 0;
      if (mappedInput.wasScreenTapped(tx, ty)) {
        const auto& metrics = UITheme::getInstance().getMetrics();
        const int top = metrics.topPadding + metrics.headerHeight + 8;
        const int rh = std::max(34, renderer.getLineHeight(UI_10_FONT_ID) * 2 + 10);
        const int margin = std::max(12, metrics.contentSidePadding);
        if (tx >= margin && tx < renderer.getScreenWidth() - margin && ty >= top) {
          const int idx = (ty - top) / rh;
          if (idx >= 0 && idx < count) { selectedResult = idx; fetchArticle(idx); return; }
        }
      }
    }
  }
}

void WikipediaActivity::render(RenderLock&&) {
  if (!needsRedraw) return;
  needsRedraw = false;
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int margin = std::max(12, metrics.contentSidePadding);
  renderer.drawCenteredText(UI_12_FONT_ID, metrics.topPadding, "Wikipedia", true, EpdFontFamily::BOLD);

  if (state == State::Searching || state == State::Fetching || state == State::Opening || state == State::Idle) {
    const char* msg = statusLine.empty() ? "Loading..." : statusLine.c_str();
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 30, msg, true);
    GUI.drawButtonHints(renderer, "Back", "", "", "");
    renderer.displayBuffer();
    return;
  }
  if (state == State::Error) {
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 30, "Error", true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 60, errorMessage.c_str(), true);
    GUI.drawButtonHints(renderer, tr(STR_BACK), tr(STR_SELECT), "", "");
    renderer.displayBuffer();
    return;
  }
  if (state == State::Results) {
    int y = metrics.topPadding + metrics.headerHeight + 8;
    const int rh = std::max(34, renderer.getLineHeight(UI_10_FONT_ID) * 2 + 10);
    const int bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 6;
    for (int i = 0; i < static_cast<int>(resultTitles.size()); ++i) {
      if (y + rh > bottom) break;
      const bool sel = (i == selectedResult);
      const int w = renderer.getScreenWidth() - margin * 2;
      renderer.drawRect(margin, y, w, rh - 4, sel ? 2 : 1, true);
      if (sel) renderer.fillRect(margin + 2, y + 2, 4, rh - 8);
      renderer.drawText(UI_10_FONT_ID, margin + 12, y + 4, resultTitles[i].c_str(), true, EpdFontFamily::BOLD);
      if (i < (int)resultDescs.size() && !resultDescs[i].empty())
        renderer.drawText(SMALL_FONT_ID, margin + 12, y + 4 + renderer.getLineHeight(UI_10_FONT_ID), resultDescs[i].c_str(), true);
      y += rh;
    }
    GUI.drawButtonHints(renderer, tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    renderer.displayBuffer();
    return;
  }
  renderer.drawCenteredText(UI_10_FONT_ID, metrics.topPadding + metrics.headerHeight + 30, "Search Wikipedia", true);
  GUI.drawButtonHints(renderer, tr(STR_BACK), "", "", "");
  renderer.displayBuffer();
}

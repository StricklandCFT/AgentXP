#include "http_sender.h"

#include <windows.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

static bool CrackUrl(const std::string& url, URL_COMPONENTSA& out, std::string& host, std::string& path) {
  ZeroMemory(&out, sizeof(out));
  out.dwStructSize = sizeof(out);

  char host_buf[256];
  char path_buf[1024];
  out.lpszHostName = host_buf;
  out.dwHostNameLength = sizeof(host_buf);
  out.lpszUrlPath = path_buf;
  out.dwUrlPathLength = sizeof(path_buf);

  if (!InternetCrackUrlA(url.c_str(), 0, 0, &out)) {
    return false;
  }
  host.assign(out.lpszHostName, out.dwHostNameLength);
  path.assign(out.lpszUrlPath, out.dwUrlPathLength);
  if (path.empty()) {
    path = "/";
  }
  return true;
}

bool HttpPostJson(const std::string& url, const std::string& payload) {
  URL_COMPONENTSA parts;
  std::string host;
  std::string path;
  if (!CrackUrl(url, parts, host, path)) {
    return false;
  }

  HINTERNET h_inet = InternetOpenA("ProcmonAgent", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
  if (!h_inet) {
    return false;
  }

  HINTERNET h_conn = InternetConnectA(h_inet, host.c_str(), parts.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
  if (!h_conn) {
    InternetCloseHandle(h_inet);
    return false;
  }

  DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES;
  HINTERNET h_req = HttpOpenRequestA(h_conn, "POST", path.c_str(), NULL, NULL, NULL, flags, 0);
  if (!h_req) {
    InternetCloseHandle(h_conn);
    InternetCloseHandle(h_inet);
    return false;
  }

  const char* headers = "Content-Type: application/json\r\n";
  BOOL ok = HttpSendRequestA(h_req, headers, lstrlenA(headers), (LPVOID)payload.data(), (DWORD)payload.size());

  InternetCloseHandle(h_req);
  InternetCloseHandle(h_conn);
  InternetCloseHandle(h_inet);

  return ok == TRUE;
}

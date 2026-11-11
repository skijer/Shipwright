#include "StoreHttp.h"

#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#elif defined(UNBOUND_STORE_CURL)
#include <curl/curl.h>
#include <mutex>
#endif

namespace {

constexpr std::string_view HttpsPrefix = "https://";
constexpr const char* UserAgent = "Unbound-ModStore";

bool IsHttps(const std::string& url) {
    return url.compare(0, HttpsPrefix.size(), HttpsPrefix) == 0;
}

StoreHttpResult Fail(std::string error) {
    StoreHttpResult result;
    result.error = std::move(error);
    return result;
}

#if defined(_WIN32)

std::wstring Widen(const std::string& text) {
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wide(size > 0 ? size - 1 : 0, L'\0');
    if (size > 1) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), size);
    }
    return wide;
}

class WinHttpHandle {
  public:
    explicit WinHttpHandle(HINTERNET handle) : mHandle(handle) {
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    ~WinHttpHandle() {
        if (mHandle != nullptr) {
            WinHttpCloseHandle(mHandle);
        }
    }
    HINTERNET Get() const {
        return mHandle;
    }

  private:
    HINTERNET mHandle;
};

StoreHttpResult Download(const std::string& url, size_t maxBytes) {
    const std::wstring wideUrl = Widen(url);
    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        return Fail("not an https address");
    }
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    const std::wstring path = std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) +
                              std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    WinHttpHandle session(WinHttpOpen(Widen(UserAgent).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (session.Get() == nullptr) {
        return Fail("could not start WinHTTP");
    }
    WinHttpHandle connection(WinHttpConnect(session.Get(), host.c_str(), parts.nPort, 0));
    WinHttpHandle request(connection.Get() == nullptr
                              ? nullptr
                              : WinHttpOpenRequest(connection.Get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                   WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (request.Get() == nullptr) {
        return Fail("could not reach " + url);
    }
    if (!WinHttpSendRequest(request.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.Get(), nullptr)) {
        return Fail("no answer from " + url);
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        return Fail("HTTP " + std::to_string(status) + " for " + url);
    }

    StoreHttpResult result;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request.Get(), &available) && available > 0) {
        if (result.body.size() + available > maxBytes) {
            return Fail(url + " is larger than expected");
        }
        const size_t offset = result.body.size();
        result.body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.Get(), result.body.data() + offset, available, &read)) {
            return Fail("the download of " + url + " was interrupted");
        }
        result.body.resize(offset + read);
    }
    return result;
}

#elif defined(UNBOUND_STORE_CURL)

struct CurlBuffer {
    std::vector<char>* body;
    size_t maxBytes;
};

size_t AppendToBuffer(char* data, size_t size, size_t count, void* userData) {
    auto* buffer = static_cast<CurlBuffer*>(userData);
    const size_t bytes = size * count;
    if (buffer->body->size() + bytes > buffer->maxBytes) {
        return 0;
    }
    buffer->body->insert(buffer->body->end(), data, data + bytes);
    return bytes;
}

StoreHttpResult Download(const std::string& url, size_t maxBytes) {
    static std::once_flag initialized;
    std::call_once(initialized, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return Fail("could not start libcurl");
    }
    StoreHttpResult result;
    CurlBuffer buffer = { &result.body, maxBytes };
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, UserAgent);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendToBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        return Fail(std::string(curl_easy_strerror(code)) + " for " + url);
    }
    if (status != 200) {
        return Fail("HTTP " + std::to_string(status) + " for " + url);
    }
    return result;
}

#else

StoreHttpResult Download(const std::string&, size_t) {
    return Fail("this build has no HTTPS client");
}

#endif

} // namespace

bool ModStore_HasHttps() {
#if defined(_WIN32) || defined(UNBOUND_STORE_CURL)
    return true;
#else
    return false;
#endif
}

StoreHttpResult ModStore_HttpsGet(const std::string& url, size_t maxBytes) {
    if (!IsHttps(url)) {
        return Fail("not an https address: " + url);
    }
    return Download(url, maxBytes);
}

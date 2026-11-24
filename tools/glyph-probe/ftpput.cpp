// Minimal Win32 FTP uploader for ReactOS guest -> host log transfer.
//
// Double-click (or run with optional local path argument) to upload a file
// to the host FTP receiver at 10.0.2.2:2121. Uses PASV so the guest (behind
// QEMU SLIRP NAT) initiates both control and data connections.
//
// No CRT file I/O, no dynamic MinGW runtime: statically linked like the
// installer GUI, so it runs on a clean ReactOS guest.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* kHost = "10.0.2.2";
constexpr unsigned short kPort = 2121;
constexpr const wchar_t* kDefaultFile = L"C:\\glyph-probe-result.txt";

// ---- socket helpers -------------------------------------------------------

bool connect_to(const char* host, unsigned short port, SOCKET* out) {
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = ::inet_addr(host);
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        ::closesocket(s);
        return false;
    }
    *out = s;
    return true;
}

// Read one FTP reply line (handles multi-line replies by requiring the final
// "NNN " line). Keeps a per-socket buffer across calls.
bool read_reply(SOCKET s, std::string& buf, std::string* last_line) {
    std::string line;
    for (;;) {
        // Return a buffered line if available.
        std::size_t nl = buf.find('\n');
        if (nl != std::string::npos) {
            line = buf.substr(0, nl);
            buf.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
        } else {
            char chunk[512];
            int n = ::recv(s, chunk, sizeof(chunk), 0);
            if (n <= 0) return false;
            buf.append(chunk, static_cast<std::size_t>(n));
            continue;
        }
        // Final line of a reply: "123 " at start (or "123-" means more).
        if (line.size() >= 4 && line[3] == ' ') {
            if (last_line) *last_line = line;
            return true;
        }
        // Otherwise it is an intermediate "123-" line; keep reading.
    }
}

bool reply_is(const std::string& line, const char* code) {
    return line.size() >= 3 && line.compare(0, 3, code, 3) == 0;
}

// We connect the PASV data socket BEFORE sending STOR (guaranteed to work
// through SLIRP NAT). pyftpdlib replies 125 "Data connection already open"
// in that order; servers that expect the classic order reply 150. Accept both.
bool reply_is_transfer_start(const std::string& line) {
    return reply_is(line, "125") || reply_is(line, "150");
}

bool send_cmd(SOCKET s, const char* cmd) {
    std::string line(cmd);
    line += "\r\n";
    return ::send(s, line.data(), static_cast<int>(line.size()), 0) ==
           static_cast<int>(line.size());
}

// Parse "227 Entering Passive Mode (a,b,c,d,p1,p2)." -> p1*256+p2.
// The data connection always targets the SAME host we connected to, ignoring
// the address in the reply (SLIRP guests must not connect to a routed IP).
bool parse_pasv_port(const std::string& line, unsigned short* out) {
    std::size_t open = line.find('(');
    std::size_t close_line = line.find(')', open == std::string::npos ? 0 : open);
    if (open == std::string::npos || close_line == std::string::npos) return false;
    int fields[6] = {0, 0, 0, 0, 0, 0};
    int idx = 0;
    const char* p = line.data() + open + 1;
    const char* end = line.data() + close_line;
    while (p < end && idx < 6) {
        if (*p >= '0' && *p <= '9') {
            fields[idx] = fields[idx] * 10 + (*p - '0');
            ++p;
        } else if (*p == ',') {
            ++idx;
            ++p;
        } else {
            ++p;
        }
    }
    if (idx != 5) return false;
    *out = static_cast<unsigned short>(fields[4] * 256 + fields[5]);
    return *out != 0;
}

// ---- file I/O (Win32 only, no CRT streams) --------------------------------

bool read_file(const wchar_t* path, std::vector<char>* out) {
    HANDLE h = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(h, &size) || size.QuadPart < 0 ||
        size.QuadPart > 16 * 1024 * 1024) {
        ::CloseHandle(h);
        return false;  // cap at 16 MiB; probe results are a few KB
    }
    out->resize(static_cast<std::size_t>(size.QuadPart));
    DWORD got = 0;
    bool ok = out->empty() ||
              (::ReadFile(h, out->data(), static_cast<DWORD>(out->size()), &got, nullptr) &&
               got == out->size());
    ::CloseHandle(h);
    return ok;
}

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return L"";
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

std::string wide_to_utf8(const std::wstring& s) {
    if (s.empty()) return "";
    int n = ::WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0,
                                  nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n,
                          nullptr, nullptr);
    return out;
}

void message(const wchar_t* title, bool ok, const std::wstring& detail) {
    std::wstring text = ok ? L"OK - uploaded:\n" : L"FAILED:\n";
    text += detail;
    ::MessageBoxW(nullptr, text.c_str(), title, ok ? MB_OK | MB_ICONINFORMATION
                                                   : MB_OK | MB_ICONERROR);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdline, int) {
    WSADATA wsa{};
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        message(L"ftpput", false, L"WSAStartup failed");
        return 1;
    }

    // Optional first argument = local file path (supports "..." quoting).
    // Manual parse avoids a shell32.dll import (ReactOS compatibility).
    std::wstring local = kDefaultFile;
    if (cmdline) {
        const wchar_t* p = cmdline;
        while (*p == L' ' || *p == L'\t') ++p;
        if (*p == L'"') {
            ++p;
            const wchar_t* start = p;
            while (*p && *p != L'"') ++p;
            if (p > start) local.assign(start, p - start);
        } else if (*p) {
            const wchar_t* start = p;
            while (*p && *p != L' ' && *p != L'\t') ++p;
            if (p > start) local.assign(start, p - start);
        }
    }

    std::vector<char> payload;
    if (!read_file(local.c_str(), &payload)) {
        message(L"ftpput", false, L"Cannot read: " + local + L"\n(run glyph-probe.exe first?)");
        ::WSACleanup();
        return 1;
    }

    SOCKET ctrl = INVALID_SOCKET;
    std::string buf;
    std::string reply;
    bool ok = false;
    std::wstring detail;

    do {
        if (!connect_to(kHost, kPort, &ctrl)) {
            detail = L"Cannot connect to " + utf8_to_wide(kHost) + L":" +
                     std::to_wstring(kPort);
            break;
        }
        if (!read_reply(ctrl, buf, &reply) || !reply_is(reply, "220")) {
            detail = L"Bad greeting: " + utf8_to_wide(reply);
            break;
        }
        if (!send_cmd(ctrl, "USER anonymous") ||
            !read_reply(ctrl, buf, &reply) || !reply_is(reply, "331")) {
            detail = L"USER rejected: " + utf8_to_wide(reply);
            break;
        }
        if (!send_cmd(ctrl, "PASS guest@ros-pinyin") ||
            !read_reply(ctrl, buf, &reply) || !reply_is(reply, "230")) {
            detail = L"PASS rejected: " + utf8_to_wide(reply);
            break;
        }
        if (!send_cmd(ctrl, "TYPE I") ||
            !read_reply(ctrl, buf, &reply) || !reply_is(reply, "200")) {
            detail = L"TYPE I failed: " + utf8_to_wide(reply);
            break;
        }
        if (!send_cmd(ctrl, "PASV") ||
            !read_reply(ctrl, buf, &reply) || !reply_is(reply, "227")) {
            detail = L"PASV failed: " + utf8_to_wide(reply);
            break;
        }
        unsigned short data_port = 0;
        if (!parse_pasv_port(reply, &data_port)) {
            detail = L"Cannot parse PASV: " + utf8_to_wide(reply);
            break;
        }
        SOCKET data = INVALID_SOCKET;
        if (!connect_to(kHost, data_port, &data)) {
            detail = L"Cannot connect data port " + std::to_wstring(data_port);
            break;
        }
        std::string stor = "STOR " + wide_to_utf8(local.substr(local.find_last_of(L"\\/") + 1));
        if (!send_cmd(ctrl, stor.c_str()) ||
            !read_reply(ctrl, buf, &reply) || !reply_is_transfer_start(reply)) {
            ::closesocket(data);
            detail = L"STOR rejected: " + utf8_to_wide(reply);
            break;
        }
        std::size_t sent = 0;
        while (sent < payload.size()) {
            int n = ::send(data, payload.data() + sent,
                           static_cast<int>(payload.size() - sent), 0);
            if (n <= 0) break;
            sent += static_cast<std::size_t>(n);
        }
        ::shutdown(data, SD_SEND);
        ::closesocket(data);
        if (sent != payload.size()) {
            detail = L"Data transfer incomplete";
            break;
        }
        if (!read_reply(ctrl, buf, &reply) || !reply_is(reply, "226")) {
            detail = L"Transfer not confirmed: " + utf8_to_wide(reply);
            break;
        }
        ok = true;
        detail = local + L"\n" + std::to_wstring(payload.size()) + L" bytes -> " +
                 utf8_to_wide(kHost) + L":" + std::to_wstring(kPort);
    } while (false);

    if (ctrl != INVALID_SOCKET) {
        send_cmd(ctrl, "QUIT");
        ::closesocket(ctrl);
    }
    ::WSACleanup();
    message(L"ftpput", ok, detail);
    return ok ? 0 : 1;
}

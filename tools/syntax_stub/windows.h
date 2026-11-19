// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Thin Windows type stub for HOST-ONLY syntax checking:
//   g++ -std=c++17 -fsyntax-only -Itools/syntax_stub ...
// It declares just enough of the Win32 surface to compile-check ime/*.cpp
// and tools/install-rospinyin/install.cpp on Linux CI. Nothing here is linked,
// shipped, or used by the real Windows/ReactOS build. Constant values are
// illustrative; the product build takes them from the real <windows.h>.

#pragma once

#include <cstddef>
#include <cstdint>

using BOOL     = int;
using DWORD    = std::uint32_t;
using UINT     = unsigned int;
using INT      = int;
using WORD     = std::uint16_t;
using BYTE     = unsigned char;
using LONG     = std::int32_t;
using LSTATUS  = LONG;
using REGSAM   = DWORD;
using WCHAR    = wchar_t;
using TCHAR    = wchar_t;

using LPWSTR   = wchar_t*;
using LPCWSTR  = const wchar_t*;
using LPTSTR   = wchar_t*;
using LPCTSTR  = const wchar_t*;
using LPBYTE   = BYTE*;
using LPVOID   = void*;
using LPCVOID  = const void*;

using WPARAM   = std::uintptr_t;
using LPARAM   = std::intptr_t;
using LRESULT  = std::intptr_t;
using LONG_PTR = std::intptr_t;
using ATOM     = std::uint16_t;
using COLORREF = DWORD;

using HANDLE    = void*;
using HINSTANCE = void*;
using HWND      = void*;
using HKL       = void*;
using HKEY      = void*;
using HDC       = void*;
using HICON     = void*;
using HCURSOR   = void*;
using HBRUSH    = void*;
using HGDIOBJ   = void*;
using HFONT     = void*;
using HMENU     = void*;

using LPSTR  = char*;
using LPCSTR = const char*;

#define WINAPI
#define CALLBACK
#define TRUE  1
#define FALSE 0

#define MAX_PATH 260
#define CS_IME   0x00001000
#define WM_CREATE  0x0001
#define WM_DESTROY 0x0002
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define DLL_THREAD_ATTACH  2
#define DLL_THREAD_DETACH  3

#define REG_SZ         1
#define REG_EXPAND_SZ  2
#define KEY_READ       0x20019
#define KEY_WRITE      0x20006
#define ERROR_SUCCESS  0
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu

// Rename-in-place and its error codes (install.cpp:move_aside).
// A mapped .ime cannot be overwritten, only renamed around.
#define MOVEFILE_REPLACE_EXISTING 0x00000001u
#define ERROR_ACCESS_DENIED       5u
#define ERROR_SHARING_VIOLATION   32u
#define GetFileExInfoStandard     0

// File I/O (imedat_loader.cpp).
#define GENERIC_READ          0x80000000u
#define FILE_SHARE_READ       0x00000001u
#define OPEN_EXISTING         3u
#define FILE_ATTRIBUTE_NORMAL 0x00000080u
#define INVALID_HANDLE_VALUE  reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(-1))
#define INVALID_FILE_SIZE     0xFFFFFFFFu

// Virtual keys used by imm_entry.cpp.
#define VK_BACK        0x08
#define VK_ESCAPE      0x1B
#define VK_SPACE       0x20
#define VK_SHIFT       0x10
#define VK_ADD         0x6B
#define VK_SUBTRACT    0x6D
#define VK_OEM_7       0xDE
#define VK_OEM_PLUS    0xBB
#define VK_OEM_COMMA   0xBC
#define VK_OEM_MINUS   0xBD
#define VK_OEM_PERIOD  0xBE

// Stock brushes handed to GetStockObject() (ui/*.cpp).
#define WHITE_BRUSH   0
#define LTGRAY_BRUSH  1
#define GRAY_BRUSH    2
#define DKGRAY_BRUSH  3
#define BLACK_BRUSH   4
#define NULL_BRUSH    5

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SPI_GETWORKAREA 0x0030
#define HWND_TOPMOST reinterpret_cast<HWND>(static_cast<std::intptr_t>(-1))
#define RGB(r, g, b) (static_cast<COLORREF>((r) | ((g) << 8) | ((b) << 16)))

// Window styles, messages and GDI metrics used by framework/platform/win32/ui.
// These used to be guessed in each production TU with #ifndef blocks (A-P2-11).
// They belong to the Win32 SDK (winuser.h / wingdi.h); the host stub supplies
// illustrative values here, while real builds take them from <windows.h>.
#define GWLP_USERDATA      (-21)
#define WS_POPUP           0x80000000L
#define WS_EX_TOOLWINDOW   0x00000080L
#define WS_EX_TOPMOST      0x00000008L
#define WS_EX_NOACTIVATE   0x08000000L
#define SW_HIDE            0
#define SW_SHOWNOACTIVATE  4
#define SWP_NOSIZE         0x0001
#define SWP_NOMOVE         0x0002
#define SWP_NOZORDER       0x0004
#define SWP_NOACTIVATE     0x0010
#define SWP_SHOWWINDOW     0x0040
#define WM_PAINT           0x000F
#define WM_ERASEBKGND      0x0014
#define WM_MOUSEACTIVATE   0x0021
#define MA_NOACTIVATE      0x0004
#define TRANSPARENT        1
#define FW_NORMAL          400
#define DEFAULT_CHARSET    1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY    1
#define DEFAULT_PITCH      8
#define FF_DONTCARE        0
#define LF_FACESIZE        32

#define HKEY_CURRENT_USER  reinterpret_cast<HKEY>(static_cast<std::uintptr_t>(0x80000001u))
#define HKEY_LOCAL_MACHINE reinterpret_cast<HKEY>(static_cast<std::uintptr_t>(0x80000002u))

struct POINT { LONG x; LONG y; };
struct SIZE  { LONG cx; LONG cy; };
struct RECT  { LONG left; LONG top; LONG right; LONG bottom; };

struct FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; };

// GetFileAttributesExW payload (tools/install-rospinyin/install.cpp:file_size).
struct WIN32_FILE_ATTRIBUTE_DATA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
};

struct PAINTSTRUCT {
    HDC   hdc;
    BOOL  fErase;
    RECT  rcPaint;
    BOOL  fRestore;
    BOOL  fIncUpdate;
    BYTE  rgbReserved[32];
};

struct CRITICAL_SECTION {
    void*         DebugInfo;
    LONG          LockCount;
    LONG          RecursionCount;
    HANDLE        OwningThread;
    HANDLE        LockSemaphore;
    std::uintptr_t SpinCount;
};

struct WNDCLASSEXW {
    UINT      cbSize;
    UINT      style;
    LRESULT (CALLBACK* lpfnWndProc)(HWND, UINT, WPARAM, LPARAM);
    INT       cbClsExtra;
    INT       cbWndExtra;
    HINSTANCE hInstance;
    HICON     hIcon;
    HCURSOR   hCursor;
    HBRUSH    hbrBackground;
    LPCWSTR   lpszMenuName;
    LPCWSTR   lpszClassName;
    HICON     hIconSm;
};

extern "C" {

BOOL   WINAPI DisableThreadLibraryCalls(HINSTANCE);
ATOM   WINAPI RegisterClassExW(const WNDCLASSEXW*);
BOOL   WINAPI UnregisterClassW(LPCWSTR, HINSTANCE);
LRESULT WINAPI DefWindowProcW(HWND, UINT, WPARAM, LPARAM);

UINT   WINAPI GetSystemDirectoryW(LPWSTR, UINT);
DWORD  WINAPI GetModuleFileNameW(HINSTANCE, LPWSTR, DWORD);
BOOL   WINAPI CopyFileW(LPCWSTR, LPCWSTR, BOOL);
BOOL   WINAPI DeleteFileW(LPCWSTR);
BOOL   WINAPI MoveFileExW(LPCWSTR, LPCWSTR, DWORD);
DWORD  WINAPI GetFileAttributesW(LPCWSTR);
BOOL   WINAPI GetFileAttributesExW(LPCWSTR, int, LPVOID);
DWORD  WINAPI GetLastError(void);

// File I/O (ime/dat_loader.cpp).
HANDLE WINAPI CreateFileW(LPCWSTR, DWORD, DWORD, LPVOID, DWORD, DWORD, HANDLE);
BOOL   WINAPI ReadFile(HANDLE, LPVOID, DWORD, DWORD*, LPVOID);
BOOL   WINAPI CloseHandle(HANDLE);
DWORD  WINAPI GetFileSize(HANDLE, DWORD*);

// Win32 critical section (ime/session_store.cpp, ui/ime_ui.cpp).
void WINAPI InitializeCriticalSection(CRITICAL_SECTION*);
void WINAPI DeleteCriticalSection(CRITICAL_SECTION*);
void WINAPI EnterCriticalSection(CRITICAL_SECTION*);
void WINAPI LeaveCriticalSection(CRITICAL_SECTION*);

// Caret / cursor anchoring (ui/ime_ui.cpp).
HWND WINAPI GetFocus(void);
BOOL WINAPI GetCaretPos(POINT*);
BOOL WINAPI GetCursorPos(POINT*);
BOOL WINAPI ClientToScreen(HWND, POINT*);
int  WINAPI GetSystemMetrics(int);

// Window + GDI surface (ui/*.cpp).
HWND     WINAPI CreateWindowExW(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int,
                                int, HWND, HMENU, HINSTANCE, LPVOID);
BOOL     WINAPI DestroyWindow(HWND);
BOOL     WINAPI ShowWindow(HWND, int);
BOOL     WINAPI SetWindowPos(HWND, HWND, int, int, int, int, UINT);
LONG_PTR WINAPI GetWindowLongPtrW(HWND, int);
LONG_PTR WINAPI SetWindowLongPtrW(HWND, int, LONG_PTR);

HDC      WINAPI GetDC(HWND);
int      WINAPI ReleaseDC(HWND, HDC);
HGDIOBJ  WINAPI SelectObject(HDC, HGDIOBJ);
BOOL     WINAPI DeleteObject(HGDIOBJ);
HFONT    WINAPI CreateFontW(int, int, int, int, int, DWORD, DWORD, DWORD,
                            DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR);
int      WINAPI GetTextFaceW(HDC, int, LPWSTR);
HDC      WINAPI BeginPaint(HWND, PAINTSTRUCT*);
BOOL     WINAPI EndPaint(HWND, const PAINTSTRUCT*);
BOOL     WINAPI GetClientRect(HWND, RECT*);
int      WINAPI FillRect(HDC, const RECT*, HBRUSH);
HGDIOBJ  WINAPI GetStockObject(int);
int      WINAPI SetBkMode(HDC, int);
COLORREF WINAPI SetTextColor(HDC, COLORREF);
BOOL     WINAPI TextOutW(HDC, int, int, LPCWSTR, int);
BOOL     WINAPI GetTextExtentPoint32W(HDC, LPCWSTR, int, SIZE*);
BOOL     WINAPI InvalidateRect(HWND, const RECT*, BOOL);

// Glyph-presence probe (ui/ime_ui.cpp GlyphProbe): a private memory DC plus
// the font currently selected, asked which code points it actually has.
HDC      WINAPI CreateCompatibleDC(HDC);
BOOL     WINAPI DeleteDC(HDC);
DWORD    WINAPI GetGlyphIndicesW(HDC, LPCWSTR, int, WORD*, DWORD);
#define GGI_MARK_NONEXISTING_GLYPHS 0x0001
#define GDI_ERROR 0xFFFFFFFFu

// Work area for the floating toolbar (ui/toolbar.cpp).
BOOL     WINAPI SystemParametersInfoW(UINT, UINT, LPVOID, UINT);

LSTATUS WINAPI RegCreateKeyExW(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM,
                               LPVOID, HKEY*, DWORD*);
LSTATUS WINAPI RegOpenKeyExW(HKEY, LPCWSTR, DWORD, REGSAM, HKEY*);
LSTATUS WINAPI RegQueryValueExW(HKEY, LPCWSTR, DWORD*, DWORD*, LPBYTE, DWORD*);
LSTATUS WINAPI RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
LSTATUS WINAPI RegDeleteKeyW(HKEY, LPCWSTR);
LSTATUS WINAPI RegDeleteValueW(HKEY, LPCWSTR);
LSTATUS WINAPI RegCloseKey(HKEY);

}  // extern "C"

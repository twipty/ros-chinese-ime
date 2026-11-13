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
using ATOM     = std::uint16_t;

using HANDLE    = void*;
using HINSTANCE = void*;
using HWND      = void*;
using HKL       = void*;
using HKEY      = void*;
using HDC       = void*;
using HICON     = void*;
using HCURSOR   = void*;
using HBRUSH    = void*;
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

#define HKEY_CURRENT_USER  reinterpret_cast<HKEY>(static_cast<std::uintptr_t>(0x80000001u))
#define HKEY_LOCAL_MACHINE reinterpret_cast<HKEY>(static_cast<std::uintptr_t>(0x80000002u))

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
DWORD  WINAPI GetFileAttributesW(LPCWSTR);
DWORD  WINAPI GetLastError(void);

LSTATUS WINAPI RegCreateKeyExW(HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM,
                               LPVOID, HKEY*, DWORD*);
LSTATUS WINAPI RegOpenKeyExW(HKEY, LPCWSTR, DWORD, REGSAM, HKEY*);
LSTATUS WINAPI RegQueryValueExW(HKEY, LPCWSTR, DWORD*, DWORD*, LPBYTE, DWORD*);
LSTATUS WINAPI RegSetValueExW(HKEY, LPCWSTR, DWORD, DWORD, const BYTE*, DWORD);
LSTATUS WINAPI RegDeleteKeyW(HKEY, LPCWSTR);
LSTATUS WINAPI RegDeleteValueW(HKEY, LPCWSTR);
LSTATUS WINAPI RegCloseKey(HKEY);

}  // extern "C"

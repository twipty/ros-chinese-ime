/* Diagnostic only: enumerate top-level windows, print class + text + rect.
 * Used to read the real RosPinyin toolbar label/geometry inside the guest,
 * since the toolbar shows only "拼" although the DLL builds "拼:开[快]".
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

static BOOL CALLBACK cb(HWND hwnd, LPARAM unused) {
    (void)unused;
    char cls[128] = {0};
    wchar_t txt[256] = {0};
    RECT r;
    GetClassNameA(hwnd, cls, sizeof(cls));
    int n = GetWindowTextW(hwnd, txt, 256);
    GetWindowRect(hwnd, &r);
    if (strstr(cls, "RosPinyin") != NULL || strstr(cls, "Toolbar") != NULL ||
        strstr(cls, "Notepad") != NULL || (n > 0 && n <= 16)) {
        char ascii[256];
        int i;
        for (i = 0; i < n && i < 255; ++i) {
            ascii[i] = (txt[i] >= 0x20 && txt[i] < 0x7f) ? (char)txt[i] : '?';
        }
        ascii[i < 255 ? i : 255] = 0;
        printf("HWND=%p cls=%s txt(len=%d)=%s rect=(%ld,%ld)-(%ld,%ld)\n",
               (void*)hwnd, cls, n, ascii,
               (long)r.left, (long)r.top, (long)r.right, (long)r.bottom);
    }
    return TRUE;
}

int main(void) {
    EnumWindows(cb, 0);
    return 0;
}

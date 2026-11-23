/* Diagnostic only: identify the window under a given screen point.
 * The grey "拼" strip at the bottom-right of the guest does not appear in
 * EnumWindows output, so query it directly with WindowFromPoint and walk the
 * owner/parent chain, printing class, text, rect and PID for each.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

static void describe(const char* tag, HWND hwnd) {
    if (!hwnd) { printf("%s: NULL\n", tag); return; }
    char cls[128] = {0};
    wchar_t txt[256] = {0};
    RECT r;
    DWORD pid = 0;
    GetClassNameA(hwnd, cls, sizeof(cls));
    int n = GetWindowTextW(hwnd, txt, 256);
    GetWindowRect(hwnd, &r);
    GetWindowThreadProcessId(hwnd, &pid);
    char ascii[256];
    int i;
    for (i = 0; i < n && i < 255; ++i)
        ascii[i] = (txt[i] >= 0x20 && txt[i] < 0x7f) ? (char)txt[i] : '?';
    ascii[i < 255 ? i : 255] = 0;
    printf("%s HWND=%p cls=%s txt(len=%d)=%s rect=(%ld,%ld)-(%ld,%ld) pid=%lu vis=%d\n",
           tag, (void*)hwnd, cls, n, ascii,
           (long)r.left, (long)r.top, (long)r.right, (long)r.bottom,
           (unsigned long)pid, IsWindowVisible(hwnd) ? 1 : 0);
}

int main(int argc, char** argv) {
    int x = 780, y = 580;
    if (argc >= 3) { x = atoi(argv[1]); y = atoi(argv[2]); }
    POINT pt; pt.x = x; pt.y = y;
    HWND h = WindowFromPoint(pt);
    printf("point=(%d,%d)\n", x, y);
    describe("hit   ", h);
    describe("parent", h ? GetParent(h) : NULL);
    describe("owner ", h ? GetWindow(h, GW_OWNER) : NULL);
    describe("desktop", GetDesktopWindow());
    return 0;
}

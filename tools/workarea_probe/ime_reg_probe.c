/* ime_reg_probe.c - read-only: enumerate keyboard layouts whose IME File
 * mentions rospinyin, print the layout key, IME File path and its size/head
 * bytes (to tell a release DLL from an old builtin-table one). */
#include <stdio.h>
#include <string.h>
#include <windows.h>

int main(void) {
    HKEY root;
    DWORD i;
    const char* base = "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, base, 0, KEY_READ, &root) != ERROR_SUCCESS) {
        printf("RegOpenKey FAIL base=%s\n", base);
        return 1;
    }
    for (i = 0; ; ++i) {
        char sub[256];
        DWORD sublen = sizeof(sub);
        if (RegEnumKeyExA(root, i, sub, &sublen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        HKEY h;
        if (RegOpenKeyExA(root, sub, 0, KEY_READ, &h) == ERROR_SUCCESS) {
            char ime[512] = {0};
            char text[256] = {0};
            DWORD imelen = sizeof(ime), textlen = sizeof(text);
            RegQueryValueExA(h, "IME File", NULL, NULL, (LPBYTE)ime, &imelen);
            RegQueryValueExA(h, "Layout Text", NULL, NULL, (LPBYTE)text, &textlen);
            if (strstr(ime, "rospinyin") != NULL || strstr(ime, "RosPinyin") != NULL) {
                char sysdir[MAX_PATH], full[MAX_PATH];
                FILE* f;
                GetSystemDirectoryA(sysdir, MAX_PATH);
                _snprintf(full, sizeof(full), "%s\\%s", sysdir, ime);
                f = fopen(full, "rb");
                if (f) {
                    unsigned char hdr[8];
                    long sz;
                    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
                    fread(hdr, 1, 8, f); fclose(f);
                    printf("REG key=%s text=%s\n  ime=%s\n  file=%s size=%ld MZ=%c%c\n",
                           sub, text, ime, full, sz, hdr[0], hdr[1]);
                } else {
                    printf("REG key=%s text=%s ime=%s\n  file=%s NOT FOUND\n",
                           sub, text, ime, full);
                }
            }
            RegCloseKey(h);
        }
    }
    RegCloseKey(root);
    return 0;
}

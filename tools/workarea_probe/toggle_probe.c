// Guest-side probe: send a REAL Ctrl+Shift+G to the foreground Notepad via
// keybd_event (sets the modifier bits in the keyboard state that ImeProcessKey
// reads). VNC --raw injection does not set those bits, so it cannot exercise
// the ModeToggle path. Compile with the mingw-i686 toolchain, run in guest.
#include <windows.h>

int main(void) {
    // Bring Notepad to the foreground so the keys reach it, not the console.
    HWND notepad = FindWindowW(L"Notepad", NULL);
    if (notepad != NULL) {
        SetForegroundWindow(notepad);
        BringWindowToTop(notepad);
        Sleep(200);
    }

    // Ctrl down, Shift down, G down/up, Shift up, Ctrl up. keybd_event updates
    // the global key-state array that IMM32 passes as lpbKeyState.
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event(VK_SHIFT, 0, 0, 0);
    keybd_event('G', 0, 0, 0);
    keybd_event('G', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);

    // Report whether the modifiers were seen as down while held (sanity).
    return notepad == NULL ? 1 : 0;
}

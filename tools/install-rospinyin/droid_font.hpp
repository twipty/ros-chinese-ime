// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Optional nightly Droid font pack: Windows-free vocabulary and decisions.
// The Win32 adapter owns the file/registry calls; this header pins the names,
// the checksum of the original font and the user-facing status text so the
// host-side tests can detect drift without a guest.

#ifndef ROSPINYIN_DROID_FONT_HPP
#define ROSPINYIN_DROID_FONT_HPP

#include <string>
#include <string_view>

namespace rospinyin {
namespace installer {

// ReactOS 0.4.16 ships this font; 0.4.17-dev nightly dropped it while the UI
// font substitution still points at the family. Same 8.3 file name is used by
// the shipped font.inf, so the optional pack must not rename it.
inline constexpr wchar_t kDroidFontFile[] = L"DROID.TTF";
inline constexpr wchar_t kDroidFontValue[] = L"Droid Sans Fallback (TrueType)";
inline constexpr wchar_t kFontsKey[] =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";
inline constexpr wchar_t kDroidOwnedValue[] = L"DroidFontOwned";

inline constexpr unsigned long long kDroidFontExpectedSize = 3081908ULL;
inline constexpr char kDroidFontExpectedSha256[] =
    "82a74d0ef369ec7979172023cfff87f04f0a617a2871c39ef6aeb0f399c5cdaf";

enum class DroidFontState : int {
    Missing = 0,
    System = 1,   // present before us: never replaced, never removed
    Owned = 2,    // installed by the optional pack: /u cleans it up
};

inline const wchar_t* droid_font_state_label(DroidFontState state) {
    switch (state) {
        case DroidFontState::Missing:
            return L"MISSING (中文界面可能显示 ? 方框)";
        case DroidFontState::System:
            return L"present (system font, not owned by RosPinyin)";
        case DroidFontState::Owned:
            return L"present (installed by RosPinyin font pack)";
    }
    return L"unknown";
}

// Symmetry: only our own optional pack is removed by /u. A font shipped by
// ReactOS stable must survive uninstall unchanged.
inline bool should_remove_droid_font(DroidFontState state) {
    return state == DroidFontState::Owned;
}

inline std::wstring_view droid_font_status_hint(DroidFontState state,
                                                bool registered) {
    if (state == DroidFontState::Missing || !registered) {
        return L"提示：nightly 缺 Droid 字体时，请先安装可选字体包 "
               L"droid-font-pack.zip（/fontinstall DROID.TTF），完成后重启。";
    }
    return L"";
}

inline std::wstring_view droid_font_usage_line() {
    return L"  /fontinstall <DROID.TTF> verify + install the optional nightly font pack\n";
}

}  // namespace installer
}  // namespace rospinyin

#endif  // ROSPINYIN_DROID_FONT_HPP

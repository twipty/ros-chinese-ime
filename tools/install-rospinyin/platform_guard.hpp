// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Strict product-name allowlist for the GUI installer. The old design asked
// "is this ReactOS?"; S1 v3 asks "is this a platform we have explicitly
// validated and registered?" — today that list has one entry, but adding a
// compatible branch (e.g. libernt) is a table edit plus real-machine evidence,
// not a special case in the GUI callback.

#ifndef ROSPINYIN_INSTALLER_PLATFORM_GUARD_HPP
#define ROSPINYIN_INSTALLER_PLATFORM_GUARD_HPP

#include <cstddef>
#include <string_view>

namespace rospinyin {
namespace installer {

// Only product names in this table may pass the GUI install gate. Keep it
// deliberately small: an unregistered platform is rejected by default.
inline constexpr std::wstring_view kSupportedProductNames[] = {L"ReactOS"};
inline constexpr std::size_t kSupportedProductNameCount =
    sizeof(kSupportedProductNames) / sizeof(kSupportedProductNames[0]);

inline bool ascii_iequals(std::wstring_view a, std::wstring_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        wchar_t ca = a[i];
        wchar_t cb = b[i];
        if (ca >= L'A' && ca <= L'Z') ca = static_cast<wchar_t>(ca + (L'a' - L'A'));
        if (cb >= L'A' && cb <= L'Z') cb = static_cast<wchar_t>(cb + (L'a' - L'A'));
        if (ca != cb) return false;
    }
    return true;
}

inline bool product_name_in_list(std::wstring_view product,
                                 const std::wstring_view* names,
                                 std::size_t count) noexcept {
    if (names == nullptr) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (ascii_iequals(product, names[i])) return true;
    }
    return false;
}

inline bool is_supported_product_name(std::wstring_view product) noexcept {
    return product_name_in_list(product, kSupportedProductNames,
                                kSupportedProductNameCount);
}

}  // namespace installer
}  // namespace rospinyin

#endif  // ROSPINYIN_INSTALLER_PLATFORM_GUARD_HPP

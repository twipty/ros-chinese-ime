// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Single source of truth for the version stamped into rospinyin.ime.
//
// Before tagging a release, bump the four numbers AND the string in one
// commit. tools/check_release_version.py compares this header against the tag
// name, and the CI release job refuses to publish when they disagree -- that
// is the only thing that keeps "the released binary is built from this tag"
// from silently becoming false. The drift that motivated it: alpha0.0.2 was
// published while this resource still said 0,0,1,0 / "alpha0.0.1".

#ifndef ROSPINYIN_VERSION_H
#define ROSPINYIN_VERSION_H

#define ROSPINYIN_VERSION_MAJOR 0
#define ROSPINYIN_VERSION_MINOR 1
#define ROSPINYIN_VERSION_PATCH 0
#define ROSPINYIN_VERSION_BUILD 0

// rc.exe / windres want the four numbers as a bare comma list.
#define ROSPINYIN_VERSION_COMMA                     \
    ROSPINYIN_VERSION_MAJOR, ROSPINYIN_VERSION_MINOR, \
    ROSPINYIN_VERSION_PATCH, ROSPINYIN_VERSION_BUILD

// Must equal the release tag exactly, e.g. "alpha0.0.2".
#define ROSPINYIN_VERSION_STRING "v0.1.0"

#endif  // ROSPINYIN_VERSION_H

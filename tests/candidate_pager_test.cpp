// RosPinyin - Simplified Chinese pinyin IME for ReactOS
// SPDX-License-Identifier: MIT
//
// Host-side tests for the 9-candidate paging logic used by the M2 UI.

#include <imfw/candidate_pager.hpp>

#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "candidate pager test failed: %s\n", what);
        ++failures;
    }
}

std::vector<imfw::Candidate> make_candidates(int count) {
    std::vector<imfw::Candidate> result;
    for (int i = 0; i < count; ++i) {
        imfw::Candidate candidate;
        candidate.text.assign(1, static_cast<char32_t>(U'一' + i));
        candidate.score = 1000 - i;
        result.push_back(std::move(candidate));
    }
    return result;
}

}  // namespace

int main() {
    using imfw::CandidatePager;

    CandidatePager empty;
    check(empty.empty(), "fresh pager is empty");
    check(empty.page_count() == 0, "empty pager has no pages");
    check(empty.page_size() == 0, "empty pager has no candidates on page");
    check(empty.page_begin() == nullptr, "empty pager has no page begin");
    check(!empty.next_page() && !empty.prev_page(), "empty pager cannot page");
    check(empty.select(0) == nullptr, "empty pager cannot select");

    CandidatePager pager;
    pager.reset(make_candidates(20));
    check(pager.total() == 20, "total is 20");
    check(pager.page_count() == 3, "20 candidates need 3 pages");
    check(pager.page() == 0, "reset starts at page zero");
    check(pager.page_size() == 9, "first page has 9 candidates");
    check(pager.select(0) != nullptr, "digit 0 is valid");
    check(pager.select(8) != nullptr, "digit 8 is valid");
    check(pager.select(9) == nullptr, "digit 9 is out of range");

    // First candidate on the first page.
    const auto* first = pager.select(0);
    check(first != nullptr && first->text == U"一", "digit 0 selects 一");

    // Page forward: 9..17, last page has 2 entries.
    check(pager.next_page(), "can go to page 2");
    check(pager.page() == 1, "page index is 1");
    check(pager.page_size() == 9, "page 2 is full");
    check(pager.select(8) != nullptr, "last digit on page 2 is valid");
    check(pager.next_page(), "can go to page 3");
    check(pager.page_size() == 2, "page 3 has 2 candidates");
    check(pager.select(0) != nullptr && pager.select(1) != nullptr,
          "digits 0 and 1 valid on last page");
    check(pager.select(2) == nullptr, "digit 2 invalid on last page");
    check(!pager.next_page(), "cannot page past the last page");

    // Page backward and clamp at zero.
    check(pager.prev_page(), "can go back to page 2");
    check(pager.prev_page(), "can go back to page 1");
    check(!pager.prev_page(), "cannot page before the first page");
    check(pager.page() == 0, "page clamped at zero");

    // Exactly one page and exactly full pages.
    pager.reset(make_candidates(9));
    check(pager.page_count() == 1 && !pager.next_page(),
          "9 candidates fit exactly one page");
    pager.reset(make_candidates(18));
    check(pager.page_count() == 2 && pager.next_page() &&
          pager.page_size() == 9 && !pager.next_page(),
          "18 candidates fit exactly two pages");

    // X2-w3: replace() is the typing path. It keeps the page the user is on,
    // and clamps when the new candidate set is shorter.
    pager.reset(make_candidates(20));
    check(pager.next_page(), "get to page 2 before typing");
    pager.replace(make_candidates(20));
    check(pager.page() == 1 && pager.page_count() == 3,
          "typing keeps page 2 instead of snapping back to page 1");
    pager.replace(make_candidates(10));
    check(pager.page() == 1 && pager.page_count() == 2,
          "a shorter list keeps page 2 when page 2 still exists");
    pager.replace(make_candidates(5));
    check(pager.page() == 0 && pager.page_count() == 1,
          "page clamps down when the new list has only one page");
    pager.replace({});
    check(pager.empty() && pager.page() == 0 && pager.page_begin() == nullptr,
          "an empty replacement is safe");

    // Reset clears paging state.
    pager.reset(make_candidates(3));
    check(pager.page() == 0 && pager.page_count() == 1 &&
          pager.page_size() == 3,
          "reset returns to the first page");

    if (failures != 0) {
        std::fprintf(stderr, "%d candidate pager test(s) failed\n", failures);
        return 1;
    }
    std::printf("RosPinyin candidate pager OK (9 per page, paging, digits)\n");
    return 0;
}

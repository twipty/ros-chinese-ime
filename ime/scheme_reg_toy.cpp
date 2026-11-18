// RosPinyin / toy scheme — SPDX-License-Identifier: MIT
// See LICENSE and NOTICE.txt at the repository root.
//
// Assembly glue for -DROSPINYIN_SCHEME=toy: the same three lines as
// ime/scheme_reg.cpp, pointing at a different scheme. That equality is the
// result X2-a0 is after — before the injection seam this file could not exist
// without also changing the framework's own definition of scheme_instance().

#include <imfw/scheme_registry.hpp>
#include <toy_scheme.hpp>

namespace imfw {

Scheme* scheme_factory() {
    static toy::ToyScheme scheme;
    return &scheme;
}

}  // namespace imfw

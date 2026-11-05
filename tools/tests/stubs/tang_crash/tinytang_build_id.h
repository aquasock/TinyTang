/* Host stub: the build identity the firmware header carries.
 * SPDX-License-Identifier: MIT */
#ifndef STUB_CR_BUILD_ID_H
#define STUB_CR_BUILD_ID_H
/* The firmware's own values are "abc1234" or "abc1234-dirty.17f81b2"; an
 * honest host build has no commit, so this says so rather than pretending. */
#define TINYTANG_BUILD_ID "host-test"
#endif

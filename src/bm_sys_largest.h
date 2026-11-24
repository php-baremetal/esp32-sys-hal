/*
   +----------------------------------------------------------------------+
   | php-baremetal: esp32-sys-hal                                         |
   +----------------------------------------------------------------------+
   | Copyright (c) 2026 Gianfrancesco Aurecchia                           |
   +----------------------------------------------------------------------+
   | This source file is subject to the MIT license, that is bundled with |
   | this package in the file LICENSE.                                    |
   +----------------------------------------------------------------------+
   | Authors: Gianfrancesco Aurecchia <gianfri@php-baremetal.com>         |
   +----------------------------------------------------------------------+
*/

/* Largest allocatable block by trying, as a pure function (no ESP-IDF), so it can be tested on the PC. */
#ifndef BM_SYS_LARGEST_H
#define BM_SYS_LARGEST_H

#include <stdbool.h>
#include <stddef.h>

/* The largest size in 1..upper for which try_alloc() (allocate and free at once) works, 0 if none: a binary search,
 * so try_alloc() must work for every size below one that works. */
static inline size_t bm_sys_largest_search(size_t upper, bool (*try_alloc)(size_t size, void *ctx), void *ctx)
{
    size_t lo = 0;
    size_t hi = upper + 1;
    while (hi - lo > 1) {
        size_t mid = lo + (hi - lo) / 2;
        if (try_alloc(mid, ctx)) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

#endif

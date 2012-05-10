/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#include <easy_string.h>

char *easy_strncpy(char *dst, const char *src, size_t n)
{
    if (!n || !dst)
        return NULL;

    const uint64_t himagic = __UINT64_C(0x8080808080808080);
    const uint64_t lomagic = __UINT64_C(0x0101010101010101);
    const uint64_t *nsrc = (const uint64_t *)src;
    const uint64_t *nend = nsrc + (--n / 8);
    uint64_t *ndst = (uint64_t *)dst;

    while(nsrc != nend) {
        uint64_t k = *nsrc;

        if (((k - lomagic) & ~k & himagic) != 0) {
            const char *cp = (const char *) nsrc;

            if (cp[0] == 0) {
                n = 0;
                break;
            }

            if (cp[1] == 0) {
                n = 1;
                break;
            }

            if (cp[2] == 0) {
                n = 2;
                break;
            }

            if (cp[3] == 0) {
                n = 3;
                break;
            }

            if (cp[4] == 0) {
                n = 4;
                break;
            }

            if (cp[5] == 0) {
                n = 5;
                break;
            }

            if (cp[6] == 0) {
                n = 6;
                break;
            }

            n = 7;
            break;
        }

        *ndst++ = k;
        nsrc ++;
    }

    const char *nsrc2 = (const char *) nsrc;

    char *ndst2 = (char *) ndst;

    switch(n & 7) {
    case 7:
        *ndst2++ = *nsrc2++;

    case 6:
        *ndst2++ = *nsrc2++;

    case 5:
        *ndst2++ = *nsrc2++;

    case 4:
        *ndst2++ = *nsrc2++;

    case 3:
        *ndst2++ = *nsrc2++;

    case 2:
        *ndst2++ = *nsrc2++;

    case 1:
        *ndst2++ = *nsrc2++;
    };

    *ndst2 = '\0';

    return dst;
}

/**
 * 把char转成hex
 */
char *easy_string_tohex(const char *str, int n, char *result, int size)
{
    int i, j = 0;
    static char hexconvtab[] = "0123456789ABCDEF";
    const unsigned char *p = (const unsigned char *)str;

    n = easy_min((size - 1) / 2, n);

    for(i = 0; i < n; i++) {
        result[j++] = hexconvtab[p[i] >> 4];
        result[j++] = hexconvtab[p[i] & 0xf];
    }

    result[j] = '\0';

    return result;
}

/**
 * 转成大写
 */
char *easy_string_toupper(char *str)
{
    char *p = str;

    while(*p) {
        if ((*p) >= 'a' && (*p) <= 'z')
            (*p) -= 36;

        p ++;
    }

    return str;
}

/**
 * 转成小写
 */
char *easy_string_tolower(char *str)
{
    char *p = str;

    while(*p) {
        if ((*p) >= 'A' && (*p) <= 'Z')
            (*p) += 36;

        p ++;
    }

    return str;
}

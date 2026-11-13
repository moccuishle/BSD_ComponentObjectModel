/* GUID 解析 / 格式化 / 比较，以及运行时预定义 IID */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bsdcom.h"

/* {00000000-0000-0000-C000-000000000046} —— IUnknown */
const BSDCOM_IID BSDCOM_IID_IUnknown = {
    0x00000000u, 0x0000u, 0x0000u,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

/* {00000001-0000-0000-C000-000000000046} —— IClassFactory */
const BSDCOM_IID BSDCOM_IID_IClassFactory = {
    0x00000001u, 0x0000u, 0x0000u,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_hex(const char *s, int digits, uint32_t *out)
{
    uint32_t v = 0;
    int i, d;
    for (i = 0; i < digits; i++) {
        d = hexval((unsigned char)s[i]);
        if (d < 0) return -1;
        v = (v << 4) | (uint32_t)d;
    }
    *out = v;
    return 0;
}

/* 接受带花括号或不带花括号的 8-4-4-4-12 形式 */
int BsdComGuidFromString(const char *text, BSDCOM_GUID *out)
{
    const char *p;
    uint32_t v;
    int i;

    if (!text || !out) return -1;
    p = text;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '{') p++;

    if (parse_hex(p, 8, &v) < 0) return -1;
    out->Data1 = v;
    p += 8;
    if (*p++ != '-') return -1;

    if (parse_hex(p, 4, &v) < 0) return -1;
    out->Data2 = (uint16_t)v;
    p += 4;
    if (*p++ != '-') return -1;

    if (parse_hex(p, 4, &v) < 0) return -1;
    out->Data3 = (uint16_t)v;
    p += 4;
    if (*p++ != '-') return -1;

    for (i = 0; i < 2; i++) {
        if (parse_hex(p, 2, &v) < 0) return -1;
        out->Data4[i] = (uint8_t)v;
        p += 2;
    }
    if (*p++ != '-') return -1;

    for (i = 2; i < 8; i++) {
        if (parse_hex(p, 2, &v) < 0) return -1;
        out->Data4[i] = (uint8_t)v;
        p += 2;
    }
    return 0;
}

void BsdComGuidToString(BSDCOM_REFGUID g, char out[40])
{
    if (!g || !out) return;
    snprintf(out, 40,
             "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             (unsigned)g->Data1, (unsigned)g->Data2, (unsigned)g->Data3,
             (unsigned)g->Data4[0], (unsigned)g->Data4[1],
             (unsigned)g->Data4[2], (unsigned)g->Data4[3],
             (unsigned)g->Data4[4], (unsigned)g->Data4[5],
             (unsigned)g->Data4[6], (unsigned)g->Data4[7]);
}

int BsdComGuidEqual(BSDCOM_REFGUID a, BSDCOM_REFGUID b)
{
    if (!a || !b) return 0;
    return memcmp(a, b, sizeof(BSDCOM_GUID)) == 0;
}

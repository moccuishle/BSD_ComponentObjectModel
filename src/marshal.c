/*
 * marshal.c - 编组层实现：字节缓冲区 + 通道 + 回环通道
 *
 * 只依赖标准 C（stdlib / string），可在 4.4BSD 与 Windows 验证环境
 * 下同样编译。字节序按主机原生处理，文档中已声明仅保证同机互通。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsdcom_rpc.h"

/* ------------------------------------------------------------------ */
/* 缓冲区                                                              */
/* ------------------------------------------------------------------ */

void BsdComBufferInit(BSDCOM_BUFFER *b)
{
    if (!b) return;
    b->data  = NULL;
    b->len   = 0;
    b->cap   = 0;
    b->pos   = 0;
    b->error = 0;
}

int BsdComBufferInitFrom(BSDCOM_BUFFER *b, const void *data, size_t len)
{
    if (!b) return -1;
    BsdComBufferInit(b);
    if (len == 0) return 0;
    b->data = (uint8_t *)malloc(len);
    if (!b->data) {
        b->error = 1;
        return -1;
    }
    memcpy(b->data, data, len);
    b->len = len;
    b->cap = len;
    return 0;
}

void BsdComBufferFree(BSDCOM_BUFFER *b)
{
    if (!b) return;
    free(b->data);
    BsdComBufferInit(b);
}

void BsdComBufferReset(BSDCOM_BUFFER *b)
{
    if (!b) return;
    b->pos   = 0;
    b->error = 0;
}

static int buf_reserve(BSDCOM_BUFFER *b, size_t extra)
{
    size_t need, ncap;
    uint8_t *p;

    if (b->error) return -1;
    need = b->len + extra;
    if (need <= b->cap) return 0;

    ncap = b->cap ? b->cap : 64;
    while (ncap < need) ncap *= 2;

    p = (uint8_t *)realloc(b->data, ncap);
    if (!p) {
        b->error = 1;
        return -1;
    }
    b->data = p;
    b->cap  = ncap;
    return 0;
}

int BsdComBufferPut(BSDCOM_BUFFER *b, const void *p, size_t n)
{
    if (!b || !p) return -1;
    if (buf_reserve(b, n) < 0) return -1;
    memcpy(b->data + b->len, p, n);
    b->len += n;
    return 0;
}

int BsdComBufferGet(BSDCOM_BUFFER *b, void *p, size_t n)
{
    if (!b || !p) return -1;
    if (b->error) return -1;
    if (b->pos + n > b->len) {   /* 数据不足，标记错误并拒绝越界读 */
        b->error = 1;
        return -1;
    }
    memcpy(p, b->data + b->pos, n);
    b->pos += n;
    return 0;
}

#define SCALAR_IO(T, SUFFIX)                                        \
    int BsdComBufferPut##SUFFIX(BSDCOM_BUFFER *b, T v)               \
    {                                                               \
        return BsdComBufferPut(b, &v, sizeof(v));                    \
    }                                                               \
    int BsdComBufferGet##SUFFIX(BSDCOM_BUFFER *b, T *v)              \
    {                                                               \
        if (!v) return -1;                                           \
        return BsdComBufferGet(b, v, sizeof(*v));                     \
    }

SCALAR_IO(int8_t,   I8)
SCALAR_IO(uint8_t,  U8)
SCALAR_IO(int16_t,  I16)
SCALAR_IO(uint16_t, U16)
SCALAR_IO(int32_t,  I32)
SCALAR_IO(uint32_t, U32)
SCALAR_IO(int64_t,  I64)
SCALAR_IO(uint64_t, U64)
SCALAR_IO(float,    F32)
SCALAR_IO(double,   F64)

int BsdComBufferPutString(BSDCOM_BUFFER *b, const char *s)
{
    uint32_t n;

    if (!b) return -1;
    if (!s) {
        n = 0;
        return BsdComBufferPutU32(b, n);
    }
    n = (uint32_t)(strlen(s) + 1);   /* 含结尾 NUL，接收侧少一次 strlen */
    if (BsdComBufferPutU32(b, n) < 0) return -1;
    return BsdComBufferPut(b, s, n);
}

int BsdComBufferGetString(BSDCOM_BUFFER *b, char *dst, size_t dst_cap)
{
    uint32_t n;
    size_t copy;

    if (!b || !dst || dst_cap == 0) return -1;
    if (BsdComBufferGetU32(b, &n) < 0) return -1;
    if (n == 0) {
        dst[0] = '\0';
        return 0;
    }
    if (b->pos + n > b->len) {
        b->error = 1;
        return -1;
    }
    copy = (n < dst_cap) ? (size_t)n : dst_cap - 1;
    memcpy(dst, b->data + b->pos, copy);
    dst[copy] = '\0';
    b->pos += n;            /* 即使被截断也要整体跳过，保持流同步 */
    return 0;
}

void BsdComBufferDump(const BSDCOM_BUFFER *b, FILE *fp)
{
    size_t i;

    if (!b || !fp) return;
    fprintf(fp, "  %u 字节:", (unsigned)b->len);
    for (i = 0; i < b->len; i++) {
        if (i % 16 == 0) fprintf(fp, "\n    %04u  ", (unsigned)i);
        fprintf(fp, "%02X ", b->data[i]);
    }
    fprintf(fp, "\n");
}

/* ------------------------------------------------------------------ */
/* 通道                                                                */
/* ------------------------------------------------------------------ */

BSDCOM_HRESULT BsdComChannelInvoke(BSDCOM_CHANNEL *ch, uint32_t method,
                                   BSDCOM_BUFFER *in, BSDCOM_BUFFER *out)
{
    if (!ch || !ch->Invoke) return BSDCOM_E_POINTER;
    return ch->Invoke(ch, method, in, out);
}

/* ------------------------------------------------------------------ */
/* 回环通道                                                            */
/* ------------------------------------------------------------------ */

typedef struct LoopbackChannel {
    BSDCOM_CHANNEL      base;     /* 必须是第一个成员，便于强制转换 */
    void               *object;
    BSDCOM_STUB_DISPATCH dispatch;
    size_t              calls;
    size_t              bytes;
    BSDCOM_BUFFER       last_in;
} LoopbackChannel;

static BSDCOM_HRESULT loopback_invoke(BSDCOM_CHANNEL *self, uint32_t method,
                                      BSDCOM_BUFFER *in, BSDCOM_BUFFER *out)
{
    LoopbackChannel *lb = (LoopbackChannel *)self;
    BSDCOM_BUFFER copy;
    BSDCOM_HRESULT hr;

    if (!in || !out) return BSDCOM_E_POINTER;

    /* 复制一份再交给存根：模拟"参数已经过了一次传输"，
     * 保证存根只能看见字节，拿不到调用方的原始内存。 */
    if (BsdComBufferInitFrom(&copy, in->data, in->len) < 0)
        return BSDCOM_E_OUTOFMEMORY;

    BsdComBufferFree(&lb->last_in);
    BsdComBufferInitFrom(&lb->last_in, in->data, in->len);

    lb->calls++;
    lb->bytes += in->len;

    hr = lb->dispatch(lb->object, method, &copy, out);

    lb->bytes += out->len;
    BsdComBufferFree(&copy);
    return hr;
}

BSDCOM_CHANNEL *BsdComCreateLoopbackChannel(void *object, BSDCOM_STUB_DISPATCH dispatch)
{
    LoopbackChannel *lb;

    if (!dispatch) return NULL;
    lb = (LoopbackChannel *)calloc(1, sizeof(*lb));
    if (!lb) return NULL;

    lb->base.ctx    = lb;
    lb->base.Invoke = loopback_invoke;
    lb->object      = object;
    lb->dispatch    = dispatch;
    BsdComBufferInit(&lb->last_in);
    return &lb->base;
}

void BsdComFreeLoopbackChannel(BSDCOM_CHANNEL *ch)
{
    LoopbackChannel *lb = (LoopbackChannel *)ch;

    if (!lb) return;
    BsdComBufferFree(&lb->last_in);
    free(lb);
}

void BsdComLoopbackStats(BSDCOM_CHANNEL *ch, size_t *calls, size_t *bytes)
{
    LoopbackChannel *lb = (LoopbackChannel *)ch;

    if (!lb) return;
    if (calls) *calls = lb->calls;
    if (bytes) *bytes = lb->bytes;
}

const BSDCOM_BUFFER *BsdComLoopbackLastRequest(BSDCOM_CHANNEL *ch)
{
    LoopbackChannel *lb = (LoopbackChannel *)ch;

    if (!lb) return NULL;
    return &lb->last_in;
}

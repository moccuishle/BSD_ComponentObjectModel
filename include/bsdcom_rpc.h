/*
 * bsdcom_rpc.h - BSD-COM 编组层（marshaling）
 *
 * 这一层负责"把参数变成字节，再把字节变回参数"。IDL 编译器生成的
 * 代理/存根代码只调用这里的 API，因此传输介质（内存回环、socketpair、
 * 共享内存）可以随时替换，生成的代码不需要重新生成。
 *
 * 线格式（wire format）：小端、紧凑排列、无对齐填充。
 *   - 定长标量：按其自然字节宽度原样写入
 *   - 字符串：u32 长度（含结尾 NUL）+ 字节内容
 *   - 一次调用：按参数声明顺序写入所有 [in] / [in,out] 参数
 *   - 返回码不走缓冲区，由通道的返回值承载
 *
 * 与 ole32 的差别：MIDL 生成的是 NDR 编码（DCE/RPC 的线上表示，含
 * 对齐与类型描述符），本实现用最简的紧凑格式，只保证同机/同 ABI 互通。
 */
#ifndef BSDCOM_RPC_H
#define BSDCOM_RPC_H

#include <stdio.h>
#include "bsdcom.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 字节缓冲区                                                          */
/* ------------------------------------------------------------------ */

typedef struct BSDCOM_BUFFER {
    uint8_t *data;
    size_t   len;     /* 已写入字节数 */
    size_t   cap;     /* 已分配容量 */
    size_t   pos;     /* 读取游标 */
    int      error;   /* 非 0 表示曾发生溢出 / 数据不足 */
} BSDCOM_BUFFER;

void BsdComBufferInit(BSDCOM_BUFFER *b);
/* 从已有字节复制一份用于读取（调用方原缓冲区不受影响） */
int  BsdComBufferInitFrom(BSDCOM_BUFFER *b, const void *data, size_t len);
void BsdComBufferFree(BSDCOM_BUFFER *b);
void BsdComBufferReset(BSDCOM_BUFFER *b);        /* 读取游标归零 */

int  BsdComBufferPut(BSDCOM_BUFFER *b, const void *p, size_t n);
int  BsdComBufferGet(BSDCOM_BUFFER *b, void *p, size_t n);

int  BsdComBufferPutI8(BSDCOM_BUFFER *b, int8_t v);
int  BsdComBufferGetI8(BSDCOM_BUFFER *b, int8_t *v);
int  BsdComBufferPutU8(BSDCOM_BUFFER *b, uint8_t v);
int  BsdComBufferGetU8(BSDCOM_BUFFER *b, uint8_t *v);
int  BsdComBufferPutI16(BSDCOM_BUFFER *b, int16_t v);
int  BsdComBufferGetI16(BSDCOM_BUFFER *b, int16_t *v);
int  BsdComBufferPutU16(BSDCOM_BUFFER *b, uint16_t v);
int  BsdComBufferGetU16(BSDCOM_BUFFER *b, uint16_t *v);
int  BsdComBufferPutI32(BSDCOM_BUFFER *b, int32_t v);
int  BsdComBufferGetI32(BSDCOM_BUFFER *b, int32_t *v);
int  BsdComBufferPutU32(BSDCOM_BUFFER *b, uint32_t v);
int  BsdComBufferGetU32(BSDCOM_BUFFER *b, uint32_t *v);
int  BsdComBufferPutI64(BSDCOM_BUFFER *b, int64_t v);
int  BsdComBufferGetI64(BSDCOM_BUFFER *b, int64_t *v);
int  BsdComBufferPutU64(BSDCOM_BUFFER *b, uint64_t v);
int  BsdComBufferGetU64(BSDCOM_BUFFER *b, uint64_t *v);
int  BsdComBufferPutF32(BSDCOM_BUFFER *b, float v);
int  BsdComBufferGetF32(BSDCOM_BUFFER *b, float *v);
int  BsdComBufferPutF64(BSDCOM_BUFFER *b, double v);
int  BsdComBufferGetF64(BSDCOM_BUFFER *b, double *v);
int  BsdComBufferPutString(BSDCOM_BUFFER *b, const char *s);
int  BsdComBufferGetString(BSDCOM_BUFFER *b, char *dst, size_t dst_cap);

/* 调试用：把缓冲区内容按十六进制打到指定流 */
void BsdComBufferDump(const BSDCOM_BUFFER *b, FILE *fp);

/* ------------------------------------------------------------------ */
/* 通道：代理与存根之间的字节传输抽象                                  */
/* ------------------------------------------------------------------ */

typedef struct BSDCOM_CHANNEL BSDCOM_CHANNEL;

struct BSDCOM_CHANNEL {
    void *ctx;      /* 具体通道的私有数据 */
    BSDCOM_HRESULT (*Invoke)(BSDCOM_CHANNEL *self, uint32_t method,
                            BSDCOM_BUFFER *in, BSDCOM_BUFFER *out);
};

BSDCOM_HRESULT BsdComChannelInvoke(BSDCOM_CHANNEL *ch, uint32_t method,
                                   BSDCOM_BUFFER *in, BSDCOM_BUFFER *out);

/* 存根派发函数：IDL 编译器为每个接口生成一个，形如 ICalc_StubDispatch */
typedef BSDCOM_HRESULT (*BSDCOM_STUB_DISPATCH)(void *object, uint32_t method,
                                               BSDCOM_BUFFER *in, BSDCOM_BUFFER *out);

/* ------------------------------------------------------------------ */
/* 回环通道：同进程内"假装跨进程"，用于验证编组代码是否正确            */
/* ------------------------------------------------------------------ */

BSDCOM_CHANNEL *BsdComCreateLoopbackChannel(void *object, BSDCOM_STUB_DISPATCH dispatch);
void            BsdComFreeLoopbackChannel(BSDCOM_CHANNEL *ch);

/* 统计：经过通道的调用次数与字节数（验证确实做了字节级编组） */
void   BsdComLoopbackStats(BSDCOM_CHANNEL *ch, size_t *calls, size_t *bytes);
/* 最近一次请求的原始字节，可直接 dump 出来看线格式 */
const BSDCOM_BUFFER *BsdComLoopbackLastRequest(BSDCOM_CHANNEL *ch);

#ifdef __cplusplus
}
#endif

#endif /* BSDCOM_RPC_H */

/*
 * impl.c - ICalc 的真实实现（服务端对象）
 *
 * 注意：这里没有手写 vtable 的接口部分——函数签名来自 idlc 生成的
 * calc_idl.h，vtable 顺序由 IDL 决定。实现者只关心语义。
 */
#include <stdlib.h>
#include <string.h>
#include "calc_idl.h"

typedef struct CalcImpl {
    const ICalcVtbl *lpVtbl;   /* ABI 要求：第一个成员 */
    long ref;
} CalcImpl;

/* ---------------------------- IUnknown ---------------------------- */

static BSDCOM_HRESULT impl_QueryInterface(ICalc *This, BSDCOM_REFIID riid, void **ppv)
{
    CalcImpl *self = (CalcImpl *)This;

    if (!ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    if (BsdComGuidEqual(riid, &BSDCOM_IID_IUnknown) ||
        BsdComGuidEqual(riid, &BSDCOM_IID_ICalc)) {
        self->ref++;
        *ppv = (void *)This;
        return BSDCOM_S_OK;
    }
    return BSDCOM_E_NOINTERFACE;
}

static BSDCOM_ULONG impl_AddRef(ICalc *This)
{
    CalcImpl *self = (CalcImpl *)This;
    return (BSDCOM_ULONG)++self->ref;
}

static BSDCOM_ULONG impl_Release(ICalc *This)
{
    CalcImpl *self = (CalcImpl *)This;
    long remaining = --self->ref;

    if (remaining <= 0) {
        free(self);
        return 0;
    }
    return (BSDCOM_ULONG)remaining;
}

/* ---------------------------- ICalc ------------------------------- */

static BSDCOM_HRESULT impl_Add(ICalc *This, int32_t a, int32_t b, int32_t *result)
{
    (void)This;
    if (!result) return BSDCOM_E_POINTER;
    *result = a + b;
    return BSDCOM_S_OK;
}

static BSDCOM_HRESULT impl_Sub(ICalc *This, int32_t a, int32_t b, int32_t *result)
{
    (void)This;
    if (!result) return BSDCOM_E_POINTER;
    *result = a - b;
    return BSDCOM_S_OK;
}

static BSDCOM_HRESULT impl_Mul(ICalc *This, int32_t a, int32_t b, int32_t *result)
{
    (void)This;
    if (!result) return BSDCOM_E_POINTER;
    *result = a * b;
    return BSDCOM_S_OK;
}

static BSDCOM_HRESULT impl_Div(ICalc *This, int32_t a, int32_t b, int32_t *result)
{
    (void)This;
    if (!result) return BSDCOM_E_POINTER;
    if (b == 0) return BSDCOM_E_INVALIDARG;
    *result = a / b;
    return BSDCOM_S_OK;
}

static BSDCOM_HRESULT impl_Lookup(ICalc *This, const char *name, int32_t *result)
{
    (void)This;
    if (!result) return BSDCOM_E_POINTER;
    if (!name) return BSDCOM_E_POINTER;

    if (strcmp(name, "answer") == 0)      *result = 42;
    else if (strcmp(name, "pi") == 0)     *result = 3;
    else if (strcmp(name, "max") == 0)    *result = 2147483647;
    else if (strcmp(name, "zero") == 0)   *result = 0;
    else return BSDCOM_E_INVALIDARG;

    return BSDCOM_S_OK;
}

static const ICalcVtbl g_calcimpl_vtbl = {
    impl_QueryInterface,
    impl_AddRef,
    impl_Release,
    impl_Add,
    impl_Sub,
    impl_Mul,
    impl_Div,
    impl_Lookup
};

ICalc *CalcImplCreate(void)
{
    CalcImpl *obj = (CalcImpl *)calloc(1, sizeof(*obj));

    if (!obj) return NULL;
    obj->lpVtbl = &g_calcimpl_vtbl;
    obj->ref = 1;
    return (ICalc *)obj;
}

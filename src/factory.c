/*
 * factory.c - 通用类工厂
 *
 * 在 COM 里每个组件模块都要自己写一份 IClassFactory。这里把工厂做成
 * 运行时提供的通用对象，组件只需要提供一个构造函数指针，样板代码归零。
 */
#include <stdlib.h>
#include <stdio.h>
#include "bsdcom.h"

typedef struct BsdComFactory {
    const BSDCOM_IClassFactoryVtbl *lpVtbl;   /* 必须是第一个成员 */
    BSDCOM_CLSID clsid;
    BSDCOM_CREATOR creator;
    long ref;
    long locks;
} BsdComFactory;

static BSDCOM_HRESULT factory_QueryInterface(BSDCOM_IClassFactory *This,
                                             BSDCOM_REFIID riid, void **ppv)
{
    BsdComFactory *self = (BsdComFactory *)This;

    if (!ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    if (BsdComGuidEqual(riid, &BSDCOM_IID_IUnknown) ||
        BsdComGuidEqual(riid, &BSDCOM_IID_IClassFactory)) {
        *ppv = This;
        self->ref++;
        return BSDCOM_S_OK;
    }
    return BSDCOM_E_NOINTERFACE;
}

static BSDCOM_ULONG factory_AddRef(BSDCOM_IClassFactory *This)
{
    BsdComFactory *self = (BsdComFactory *)This;
    return (BSDCOM_ULONG)++self->ref;
}

static BSDCOM_ULONG factory_Release(BSDCOM_IClassFactory *This)
{
    BsdComFactory *self = (BsdComFactory *)This;
    long remaining = --self->ref;
    if (remaining <= 0) free(self);
    return (BSDCOM_ULONG)remaining;
}

static BSDCOM_HRESULT factory_CreateInstance(BSDCOM_IClassFactory *This,
                                             BSDCOM_IUnknown *outer,
                                             BSDCOM_REFIID riid, void **ppv)
{
    BsdComFactory *self = (BsdComFactory *)This;

    if (!ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    /* v1 不支持聚合 —— 与 ole32 对不支持聚合的组件的行为一致 */
    if (outer) return BSDCOM_CLASS_E_NOAGGREGATION;

    return self->creator(outer, riid, ppv);
}

static BSDCOM_HRESULT factory_LockServer(BSDCOM_IClassFactory *This, BSDCOM_BOOL lock)
{
    BsdComFactory *self = (BsdComFactory *)This;
    self->locks += lock ? 1 : -1;
    if (self->locks < 0) self->locks = 0;
    return BSDCOM_S_OK;
}

static const BSDCOM_IClassFactoryVtbl g_factory_vtbl = {
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    factory_CreateInstance,
    factory_LockServer
};

BSDCOM_HRESULT BsdComCreateFactory(BSDCOM_REFCLSID clsid, BSDCOM_CREATOR creator,
                                   BSDCOM_REFIID riid, void **ppv)
{
    BsdComFactory *f;

    if (!clsid || !creator || !ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    f = (BsdComFactory *)calloc(1, sizeof(*f));
    if (!f) return BSDCOM_E_OUTOFMEMORY;

    f->lpVtbl = &g_factory_vtbl;
    f->clsid = *clsid;
    f->creator = creator;
    f->ref = 0;      /* QueryInterface 会把引用计数置为 1，交给调用方持有 */
    f->locks = 0;

    return factory_QueryInterface((BSDCOM_IClassFactory *)f, riid, ppv);
}

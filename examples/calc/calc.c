/*
 * calc.c - 一个最小的 BSD-COM 进程内组件
 *
 * 编译成 libcalc.so。导出两个符号：
 *   DllGetClassObject  —— 运行时据此取得类工厂
 *   DllCanUnloadNow    —— 运行时据此判断模块能否卸载
 * 在 ELF 上符号默认可见，无需 __declspec(dllexport)。
 */
#include <stdlib.h>
#include <stdio.h>
#include "calciface.h"

typedef struct Calc {
    const ICalcVtbl *lpVtbl;   /* 必须是第一个成员，ABI 要求 */
    long ref;
} Calc;

static long g_live_objects = 0;

/* ---------------------------- IUnknown ---------------------------- */

static BSDCOM_HRESULT calc_QueryInterface(ICalc *This, BSDCOM_REFIID riid, void **ppv)
{
    Calc *self = (Calc *)This;

    if (!ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    if (BsdComGuidEqual(riid, &BSDCOM_IID_IUnknown) ||
        BsdComGuidEqual(riid, &BSDCOM_IID_ICalc)) {
        self->ref++;
        *ppv = This;
        return BSDCOM_S_OK;
    }
    return BSDCOM_E_NOINTERFACE;
}

static BSDCOM_ULONG calc_AddRef(ICalc *This)
{
    Calc *self = (Calc *)This;
    return (BSDCOM_ULONG)++self->ref;
}

static BSDCOM_ULONG calc_Release(ICalc *This)
{
    Calc *self = (Calc *)This;
    long remaining = --self->ref;

    if (remaining <= 0) {
        g_live_objects--;
        free(self);
        return 0;
    }
    return (BSDCOM_ULONG)remaining;
}

/* ---------------------------- ICalc ------------------------------- */

static BSDCOM_HRESULT calc_Add(ICalc *This, int32_t a, int32_t b, int32_t *out)
{
    (void)This;
    if (!out) return BSDCOM_E_POINTER;
    *out = a + b;
    return BSDCOM_S_OK;
}

static BSDCOM_HRESULT calc_Mul(ICalc *This, int32_t a, int32_t b, int32_t *out)
{
    (void)This;
    if (!out) return BSDCOM_E_POINTER;
    *out = a * b;
    return BSDCOM_S_OK;
}

static const ICalcVtbl g_calc_vtbl = {
    calc_QueryInterface,
    calc_AddRef,
    calc_Release,
    calc_Add,
    calc_Mul
};

/* --------------------------- 构造与导出 --------------------------- */

static BSDCOM_HRESULT Calc_Create(BSDCOM_IUnknown *outer, BSDCOM_REFIID riid, void **ppv)
{
    Calc *obj;
    BSDCOM_HRESULT hr;

    if (!ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;
    if (outer) return BSDCOM_CLASS_E_NOAGGREGATION;

    obj = (Calc *)calloc(1, sizeof(*obj));
    if (!obj) return BSDCOM_E_OUTOFMEMORY;

    obj->lpVtbl = &g_calc_vtbl;
    obj->ref = 0;          /* 由 QueryInterface 置为 1 */
    g_live_objects++;

    hr = calc_QueryInterface((ICalc *)obj, riid, ppv);
    if (BSDCOM_FAILED(hr)) {
        g_live_objects--;
        free(obj);
    }
    return hr;
}

BSDCOM_EXPORT BSDCOM_HRESULT DllGetClassObject(BSDCOM_REFCLSID clsid, BSDCOM_REFIID riid, void **ppv)
{
    if (!clsid || !riid || !ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    if (!BsdComGuidEqual(clsid, &BSDCOM_CLSID_Calc)) {
        return BSDCOM_CLASS_E_CLASSNOTAVAILABLE;
    }
    return BsdComCreateFactory(&BSDCOM_CLSID_Calc, Calc_Create, riid, ppv);
}

BSDCOM_EXPORT BSDCOM_HRESULT DllCanUnloadNow(void)
{
    return (g_live_objects == 0) ? BSDCOM_S_OK : BSDCOM_S_FALSE;
}

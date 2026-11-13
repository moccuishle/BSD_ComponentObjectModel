/*
 * coinit.c - 激活与生命周期
 *
 * 对应 ole32 的 CoInitialize / CoCreateInstance / CoGetClassObject。
 * v1 只有一个"套间"（单线程模型），跨线程/跨进程编组留到 RPC 阶段。
 */
#include <stdio.h>
#include "bsdcom.h"
#include "internal.h"

static int g_init_count = 0;
static int g_registry_loaded = 0;

BSDCOM_HRESULT CoInitialize(void *reserved)
{
    (void)reserved;

    if (g_init_count == 0 && !g_registry_loaded) {
        BsdComRegistryLoadDefault();
        g_registry_loaded = 1;   /* 即使没找到文件也标记，避免反复刷错误 */
    }
    g_init_count++;
    return BSDCOM_S_OK;
}

void CoUninitialize(void)
{
    if (g_init_count <= 0) return;
    g_init_count--;
    if (g_init_count == 0) {
        BsdComLoaderShutdown();
    }
}

BSDCOM_HRESULT CoGetClassObject(BSDCOM_REFCLSID clsid, BSDCOM_DWORD clsctx,
                                BSDCOM_REFIID riid, void **ppv)
{
    BsdComClassEntry *entry;
    char text[40];

    if (!clsid || !riid || !ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    if (g_init_count <= 0) return BSDCOM_CO_E_NOTINITIALIZED;
    if (!(clsctx & BSDCOM_CLSCTX_INPROC_SERVER)) {
        /* 本地服务器（跨进程）尚未实现 */
        return BSDCOM_CLASS_E_CLASSNOTAVAILABLE;
    }

    entry = BsdComRegistryFind(clsid);
    if (!entry) {
        BsdComGuidToString(clsid, text);
        fprintf(stderr, "bsdcom: CLSID %s 未注册\n", text);
        return BSDCOM_REGDB_E_CLASSNOTREGISTERED;
    }
    return BsdComLoaderGetClassObject(entry, riid, ppv);
}

BSDCOM_HRESULT CoCreateInstance(BSDCOM_REFCLSID clsid, BSDCOM_IUnknown *outer,
                                BSDCOM_DWORD clsctx, BSDCOM_REFIID riid, void **ppv)
{
    BSDCOM_IClassFactory *factory = NULL;
    BSDCOM_HRESULT hr;

    if (!clsid || !riid || !ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    hr = CoGetClassObject(clsid, clsctx, &BSDCOM_IID_IClassFactory, (void **)&factory);
    if (BSDCOM_FAILED(hr)) return hr;

    hr = factory->lpVtbl->CreateInstance(factory, outer, riid, ppv);
    factory->lpVtbl->Release(factory);
    return hr;
}

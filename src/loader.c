/*
 * loader.c - 进程内组件加载
 *
 * 对应 ole32 的 InprocServer 激活路径：
 *   LoadLibrary(模块) -> GetProcAddress(DllGetClassObject) -> 取得类工厂
 * 在 BSD 上就是 dlopen / dlsym。
 */
#include <stdio.h>
#include <dlfcn.h>
#include "bsdcom.h"
#include "internal.h"

typedef BSDCOM_HRESULT (*GETCLASSOBJECT_FN)(BSDCOM_REFCLSID, BSDCOM_REFIID, void **);

BSDCOM_HRESULT BsdComLoaderGetClassObject(BsdComClassEntry *entry,
                                          BSDCOM_REFIID riid, void **ppv)
{
    GETCLASSOBJECT_FN fn;
    void *sym;
    BSDCOM_HRESULT hr;

    if (!entry || !riid || !ppv) return BSDCOM_E_POINTER;
    *ppv = NULL;

    if (!entry->dlhandle) {
        entry->dlhandle = dlopen(entry->module, RTLD_NOW | RTLD_LOCAL);
        if (!entry->dlhandle) {
            fprintf(stderr, "bsdcom: dlopen(%s) 失败: %s\n", entry->module, dlerror());
            return BSDCOM_CLASS_E_CLASSNOTAVAILABLE;
        }
    }

    /* dlsym 返回 void*，转函数指针要走 memcpy 或中间变量，
     * 直接强转在严格别名规则下是未定义行为 */
    sym = dlsym(entry->dlhandle, BSDCOM_DLL_GETCLASSOBJECT);
    if (!sym) {
        fprintf(stderr, "bsdcom: %s 未导出 %s\n", entry->module, BSDCOM_DLL_GETCLASSOBJECT);
        return BSDCOM_CLASS_E_CLASSNOTAVAILABLE;
    }
    fn = (GETCLASSOBJECT_FN)sym;

    hr = fn(&entry->clsid, riid, ppv);
    if (BSDCOM_FAILED(hr)) {
        fprintf(stderr, "bsdcom: DllGetClassObject 返回 0x%08X\n", (unsigned)hr);
    }
    return hr;
}

void BsdComLoaderUnload(BsdComClassEntry *entry)
{
    if (!entry || !entry->dlhandle) return;
    dlclose(entry->dlhandle);
    entry->dlhandle = NULL;
}

static void close_one(BsdComClassEntry *e)
{
    BsdComLoaderUnload(e);
}

void BsdComLoaderShutdown(void)
{
    BsdComRegistryForEach(close_one);
}

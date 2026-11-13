/* 运行时内部接口，不对外暴露 */
#ifndef BSDCOM_INTERNAL_H
#define BSDCOM_INTERNAL_H

#include "bsdcom.h"

typedef struct BsdComClassEntry {
    BSDCOM_CLSID clsid;
    char *module;      /* .so 绝对路径 */
    char *desc;        /* 描述，调试输出用 */
    void *dlhandle;    /* dlopen 句柄，懒加载 */
    struct BsdComClassEntry *next;
} BsdComClassEntry;

BsdComClassEntry *BsdComRegistryFind(BSDCOM_REFCLSID clsid);
void BsdComRegistryForEach(void (*fn)(BsdComClassEntry *entry));
BSDCOM_HRESULT BsdComLoaderGetClassObject(BsdComClassEntry *entry,
                                          BSDCOM_REFIID riid, void **ppv);
void BsdComLoaderUnload(BsdComClassEntry *entry);
void BsdComLoaderShutdown(void);

#endif /* BSDCOM_INTERNAL_H */

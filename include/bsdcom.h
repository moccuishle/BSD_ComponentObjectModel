/*
 * bsdcom.h - BSD-COM: 一个面向 4.4BSD / BSD45 的 COM 风格组件运行时
 *
 * 设计目标（与 ole32 的对应关系见 README.md）：
 *   - 二进制接口稳定：vtable 布局固定，纯 C 实现，无 name mangling
 *   - 生命周期明确：AddRef / Release 显式引用计数
 *   - 位置透明：调用方只通过 CLSID + IID 请求对象，不关心实现在哪
 *
 * 本文件只依赖 POSIX 头文件，不包含任何 Windows 专有定义。
 */
#ifndef BSDCOM_H
#define BSDCOM_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* 组件模块入口的导出修饰：ELF（BSD）默认导出，PE（Windows 验证环境）需显式声明 */
#if defined(_WIN32)
#define BSDCOM_EXPORT __declspec(dllexport)
#else
#define BSDCOM_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 基础类型                                                            */
/* ------------------------------------------------------------------ */

typedef int32_t  BSDCOM_HRESULT;
typedef uint32_t BSDCOM_ULONG;
typedef uint32_t BSDCOM_DWORD;
typedef int32_t  BSDCOM_BOOL;

/* GUID / IID / CLSID：16 字节，与 Windows GUID 内存布局一致，
 * 便于将来与 Wine 或 Windows 侧的服务做互操作。 */
typedef struct BSDCOM_GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} BSDCOM_GUID;

typedef BSDCOM_GUID BSDCOM_IID;
typedef BSDCOM_GUID BSDCOM_CLSID;

typedef const BSDCOM_GUID *BSDCOM_REFGUID;
typedef const BSDCOM_IID  *BSDCOM_REFIID;
typedef const BSDCOM_CLSID *BSDCOM_REFCLSID;

/* ------------------------------------------------------------------ */
/* 返回码                                                              */
/* ------------------------------------------------------------------ */

#define BSDCOM_S_OK                      ((BSDCOM_HRESULT)0)
#define BSDCOM_S_FALSE                   ((BSDCOM_HRESULT)1)
#define BSDCOM_E_NOTIMPL                 ((BSDCOM_HRESULT)0x80004001u)
#define BSDCOM_E_NOINTERFACE             ((BSDCOM_HRESULT)0x80004002u)
#define BSDCOM_E_POINTER                 ((BSDCOM_HRESULT)0x80004003u)
#define BSDCOM_E_FAIL                    ((BSDCOM_HRESULT)0x80004005u)
#define BSDCOM_E_OUTOFMEMORY             ((BSDCOM_HRESULT)0x8007000Eu)
#define BSDCOM_E_INVALIDARG              ((BSDCOM_HRESULT)0x80070057u)
#define BSDCOM_E_UNEXPECTED              ((BSDCOM_HRESULT)0x8000FFFFu)
#define BSDCOM_CLASS_E_NOAGGREGATION     ((BSDCOM_HRESULT)0x80040110u)
#define BSDCOM_CLASS_E_CLASSNOTAVAILABLE ((BSDCOM_HRESULT)0x80040111u)
#define BSDCOM_REGDB_E_CLASSNOTREGISTERED ((BSDCOM_HRESULT)0x80040154u)
#define BSDCOM_CO_E_NOTINITIALIZED        ((BSDCOM_HRESULT)0x800401F0u)

#define BSDCOM_SUCCEEDED(hr) (((BSDCOM_HRESULT)(hr)) >= 0)
#define BSDCOM_FAILED(hr)    (((BSDCOM_HRESULT)(hr)) < 0)

/* 类上下文：v1 只支持进程内服务器，跨进程留作 RPC 阶段的扩展点 */
#define BSDCOM_CLSCTX_INPROC_SERVER  0x00000001u
#define BSDCOM_CLSCTX_LOCAL_SERVER   0x00000004u
#define BSDCOM_CLSCTX_ALL            0x00000017u

/* ------------------------------------------------------------------ */
/* IUnknown —— 组件模型的地基                                          */
/* ------------------------------------------------------------------ */

typedef struct BSDCOM_IUnknown BSDCOM_IUnknown;

typedef struct BSDCOM_IUnknownVtbl {
    BSDCOM_HRESULT (*QueryInterface)(BSDCOM_IUnknown *This, BSDCOM_REFIID riid, void **ppv);
    BSDCOM_ULONG   (*AddRef)(BSDCOM_IUnknown *This);
    BSDCOM_ULONG   (*Release)(BSDCOM_IUnknown *This);
} BSDCOM_IUnknownVtbl;

struct BSDCOM_IUnknown {
    const BSDCOM_IUnknownVtbl *lpVtbl;
};

/* 声明接口的宏：
 *
 *   BSDCOM_INTERFACE(ICalc) {
 *       BSDCOM_HRESULT (*QueryInterface)(ICalc *, BSDCOM_REFIID, void **);
 *       BSDCOM_ULONG   (*AddRef)(ICalc *);
 *       BSDCOM_ULONG   (*Release)(ICalc *);
 *       BSDCOM_HRESULT (*Add)(ICalc *, int32_t, int32_t, int32_t *);
 *   };
 *
 * 接口方法必须按 QueryInterface/AddRef/Release 开头排列，这是 ABI 约定。
 */
#define BSDCOM_INTERFACE(name)          \
    typedef struct name name;           \
    typedef struct name##Vtbl name##Vtbl; \
    struct name { const name##Vtbl *lpVtbl; }; \
    struct name##Vtbl

/* IID 常量：使用 static const，各编译单元各持一份只读副本。
 * 比较一律按值（BsdComGuidEqual），不要比较指针。 */
#define BSDCOM_DEFINE_GUID(name, d1, d2, d3, b0, b1, b2, b3, b4, b5, b6, b7) \
    static const BSDCOM_GUID name = {                                      \
        (uint32_t)(d1), (uint16_t)(d2), (uint16_t)(d3),                    \
        { (uint8_t)(b0), (uint8_t)(b1), (uint8_t)(b2), (uint8_t)(b3),      \
          (uint8_t)(b4), (uint8_t)(b5), (uint8_t)(b6), (uint8_t)(b7) }     \
    }

/* ------------------------------------------------------------------ */
/* 类工厂                                                              */
/* ------------------------------------------------------------------ */

BSDCOM_INTERFACE(BSDCOM_IClassFactory) {
    BSDCOM_HRESULT (*QueryInterface)(BSDCOM_IClassFactory *, BSDCOM_REFIID, void **);
    BSDCOM_ULONG   (*AddRef)(BSDCOM_IClassFactory *);
    BSDCOM_ULONG   (*Release)(BSDCOM_IClassFactory *);
    BSDCOM_HRESULT (*CreateInstance)(BSDCOM_IClassFactory *, BSDCOM_IUnknown *outer,
                                     BSDCOM_REFIID riid, void **ppv);
    BSDCOM_HRESULT (*LockServer)(BSDCOM_IClassFactory *, BSDCOM_BOOL lock);
};

/* 组件实现者只需提供这个工厂函数，工厂对象由运行时代劳 */
typedef BSDCOM_HRESULT (*BSDCOM_CREATOR)(BSDCOM_IUnknown *outer, BSDCOM_REFIID riid, void **ppv);

/* 运行时预定义的 IID：所有组件都必须能回答这两个 */
extern const BSDCOM_IID BSDCOM_IID_IUnknown;
extern const BSDCOM_IID BSDCOM_IID_IClassFactory;

BSDCOM_HRESULT BsdComCreateFactory(BSDCOM_REFCLSID clsid, BSDCOM_CREATOR creator,
                                   BSDCOM_REFIID riid, void **ppv);

/* ------------------------------------------------------------------ */
/* GUID 工具                                                           */
/* ------------------------------------------------------------------ */

int  BsdComGuidFromString(const char *text, BSDCOM_GUID *out);   /* "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}" */
void BsdComGuidToString(BSDCOM_REFGUID guid, char out[40]);
int  BsdComGuidEqual(BSDCOM_REFGUID a, BSDCOM_REFGUID b);

/* ------------------------------------------------------------------ */
/* 类注册表（对应 Windows 注册表里的 HKCR\CLSID\{...}\InprocServer32）  */
/* ------------------------------------------------------------------ */

BSDCOM_HRESULT BsdComRegisterClass(BSDCOM_REFCLSID clsid, const char *module_path,
                                   const char *description);
BSDCOM_HRESULT BsdComUnregisterClass(BSDCOM_REFCLSID clsid);
BSDCOM_HRESULT BsdComRegistryLoadFile(const char *path);  /* 逐行解析注册脚本 */
BSDCOM_HRESULT BsdComRegistryLoadDefault(void);           /* 依次尝试 /etc/bsdcom.reg 等 */
void           BsdComRegistryDump(void);                  /* 调试用 */

/* ------------------------------------------------------------------ */
/* 激活 / 生命周期（对应 CoInitialize / CoCreateInstance）             */
/* ------------------------------------------------------------------ */

BSDCOM_HRESULT CoInitialize(void *reserved);
void           CoUninitialize(void);

BSDCOM_HRESULT CoGetClassObject(BSDCOM_REFCLSID clsid, BSDCOM_DWORD clsctx,
                                BSDCOM_REFIID riid, void **ppv);

BSDCOM_HRESULT CoCreateInstance(BSDCOM_REFCLSID clsid, BSDCOM_IUnknown *outer,
                                BSDCOM_DWORD clsctx, BSDCOM_REFIID riid, void **ppv);

/* 组件模块必须导出的入口（对应 DllGetClassObject / DllCanUnloadNow） */
#define BSDCOM_DLL_GETCLASSOBJECT "DllGetClassObject"
#define BSDCOM_DLL_CANUNLOADNOW   "DllCanUnloadNow"

#ifdef __cplusplus
}
#endif

#endif /* BSDCOM_H */

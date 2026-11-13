/*
 * compat/win32/dlfcn.h - 仅用于在 Windows 上验证 BSD-COM 的逻辑
 *
 * BSD 目标平台使用系统原生 <dlfcn.h>。此垫片把语义映射到 Win32。
 *
 * 注意：这里刻意不 include <windows.h>。因为 windows.h 会连带引入
 * objbase.h / combaseapi.h，其中声明了真正的 CoInitialize、CoCreateInstance、
 * CoGetClassObject，与本项目的同名 API 直接冲突。
 * 因此只手工声明 kernel32 的三个导出符号。
 */
#ifndef BSDCOM_COMPAT_WIN32_DLFCN_H
#define BSDCOM_COMPAT_WIN32_DLFCN_H

#include <stdint.h>

#ifdef _WIN32
__declspec(dllimport) void *__stdcall LoadLibraryA(const char *lpLibFileName);
__declspec(dllimport) void *__stdcall GetProcAddress(void *hModule, const char *lpProcName);
__declspec(dllimport) int   __stdcall FreeLibrary(void *hLibModule);
#endif

#ifndef RTLD_NOW
#define RTLD_NOW 0
#endif
#ifndef RTLD_LAZY
#define RTLD_LAZY 0
#endif
#ifndef RTLD_LOCAL
#define RTLD_LOCAL 0
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 0
#endif

static __inline void *bsdcom_dlopen(const char *file, int mode)
{
    (void)mode;
    return LoadLibraryA(file);
}

static __inline void *bsdcom_dlsym(void *handle, const char *name)
{
    return GetProcAddress(handle, name);
}

static __inline int bsdcom_dlclose(void *handle)
{
    return FreeLibrary(handle) ? 0 : -1;
}

static __inline const char *bsdcom_dlerror(void)
{
    return "win32 dlopen shim: 模块加载失败";
}

#define dlopen(path, mode)   bsdcom_dlopen((path), (mode))
#define dlsym(handle, name)  bsdcom_dlsym((handle), (name))
#define dlclose(handle)      bsdcom_dlclose((handle))
#define dlerror()            bsdcom_dlerror()

#endif /* BSDCOM_COMPAT_WIN32_DLFCN_H */

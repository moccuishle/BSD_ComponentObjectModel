/*
 * demo.c - 通过"代理 → 编组 → 通道 → 存根 → 真实对象"完整走一遍调用
 *
 * 客户端只持有 ICalc（IDL 生成）和通道，拿不到 CalcImpl 的任何类型信息。
 * 参数确实被写成了字节又读回来，dump 出来的就是线上格式。
 */
#include <stdio.h>
#include <string.h>
#include "calc_idl.h"

extern ICalc *CalcImplCreate(void);

static void show(const char *what, BSDCOM_HRESULT hr, int32_t value)
{
    if (BSDCOM_SUCCEEDED(hr)) {
        printf("  %-22s = %d\n", what, (int)value);
    } else {
        printf("  %-22s 失败 HRESULT=0x%08X\n", what, (unsigned)hr);
    }
}

int main(void)
{
    ICalc *real;
    ICalc *proxy = NULL;
    BSDCOM_CHANNEL *ch;
    BSDCOM_HRESULT hr;
    int32_t r = 0;
    size_t calls = 0, bytes = 0;
    void *pv = NULL;

    printf("BSD-COM IDL 编译器演示\n");
    printf("  接口 ICalc 的 vtable 与编组代码由 idlc 从 calc.idl 生成\n");
    printf("  方法索引: Add=%d Sub=%d Mul=%d Div=%d Lookup=%d\n\n",
           ICalc_METHOD_Add, ICalc_METHOD_Sub, ICalc_METHOD_Mul,
           ICalc_METHOD_Div, ICalc_METHOD_Lookup);

    /* 1. 服务端对象 */
    real = CalcImplCreate();
    if (!real) {
        printf("创建实现对象失败\n");
        return 1;
    }

    /* 2. 通道（这里用进程内回环，换成 socketpair 就是跨进程） */
    ch = BsdComCreateLoopbackChannel(real, ICalc_StubDispatch);
    if (!ch) {
        printf("创建通道失败\n");
        return 1;
    }

    /* 3. 代理：客户端拿到的是它，而不是真实对象 */
    hr = ICalc_CreateProxy(ch, &BSDCOM_IID_ICalc, (void **)&proxy);
    if (BSDCOM_FAILED(hr)) {
        printf("创建代理失败 HRESULT=0x%08X\n", (unsigned)hr);
        return 1;
    }

    printf("代理已建立，开始调用（每次调用都会经过字节编组）\n\n");

    hr = proxy->lpVtbl->Add(proxy, 40, 2, &r);
    show("Add(40, 2)", hr, r);

    hr = proxy->lpVtbl->Sub(proxy, 40, 2, &r);
    show("Sub(40, 2)", hr, r);

    hr = proxy->lpVtbl->Mul(proxy, 6, 7, &r);
    show("Mul(6, 7)", hr, r);

    hr = proxy->lpVtbl->Div(proxy, 84, 2, &r);
    show("Div(84, 2)", hr, r);

    /* 错误路径：出参不应被写出 */
    r = -12345;
    hr = proxy->lpVtbl->Div(proxy, 1, 0, &r);
    printf("  Div(1, 0)              预期失败，实际 HRESULT=0x%08X，出参保持 %d\n",
           (unsigned)hr, (int)r);

    hr = proxy->lpVtbl->Lookup(proxy, "answer", &r);
    show("Lookup(\"answer\")", hr, r);

    hr = proxy->lpVtbl->Lookup(proxy, "max", &r);
    show("Lookup(\"max\")", hr, r);

    /* QueryInterface 走代理同样有效 */
    hr = proxy->lpVtbl->QueryInterface(proxy, &BSDCOM_IID_IUnknown, &pv);
    printf("  QueryInterface(IUnknown)  HRESULT=0x%08X, 指针%s\n",
           (unsigned)hr, pv ? "非空" : "为空");
    if (pv) ((BSDCOM_IUnknown *)pv)->lpVtbl->Release((BSDCOM_IUnknown *)pv);

    /* 线上字节：证明参数真的被序列化过 */
    BsdComLoopbackStats(ch, &calls, &bytes);
    printf("\n通道统计: %u 次调用, 共 %u 字节\n",
           (unsigned)calls, (unsigned)bytes);
    printf("最近一次请求的线格式:");
    BsdComBufferDump(BsdComLoopbackLastRequest(ch), stdout);

    /* 清理 */
    proxy->lpVtbl->Release(proxy);
    BsdComFreeLoopbackChannel(ch);
    real->lpVtbl->Release(real);

    printf("\n完成。\n");
    return 0;
}

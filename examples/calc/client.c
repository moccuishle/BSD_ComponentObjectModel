/*
 * client.c - 客户端：只知道 CLSID 和 IID，不知道实现在哪
 */
#include <stdio.h>
#include "calciface.h"

int main(void)
{
    ICalc *calc = NULL;
    BSDCOM_HRESULT hr;
    int32_t r = 0;
    char clsid_text[40];

    BsdComGuidToString(&BSDCOM_CLSID_Calc, clsid_text);
    printf("BSD-COM 演示：按 CLSID 激活组件 %s\n", clsid_text);
    printf("（客户端只链接 libbsdcom.a，不含任何 calc 实现）\n\n");

    hr = CoInitialize(NULL);
    if (BSDCOM_FAILED(hr)) {
        fprintf(stderr, "CoInitialize 失败: 0x%08X\n", (unsigned)hr);
        return 1;
    }

    hr = CoCreateInstance(&BSDCOM_CLSID_Calc, NULL, BSDCOM_CLSCTX_INPROC_SERVER,
                          &BSDCOM_IID_ICalc, (void **)&calc);
    if (BSDCOM_FAILED(hr)) {
        fprintf(stderr, "CoCreateInstance 失败: 0x%08X\n", (unsigned)hr);
        CoUninitialize();
        return 2;
    }
    printf("对象已创建: %p\n", (void *)calc);

    if (BSDCOM_FAILED(calc->lpVtbl->Add(calc, 40, 2, &r))) return 3;
    printf("40 + 2 = %d\n", r);

    if (BSDCOM_FAILED(calc->lpVtbl->Mul(calc, 6, 7, &r))) return 4;
    printf("6 * 7  = %d\n", r);

    printf("\nAddRef 后计数: %ld\n", (long)calc->lpVtbl->AddRef(calc));
    calc->lpVtbl->Release(calc);
    printf("Release 一次后对象仍存活\n");
    calc->lpVtbl->Release(calc);
    printf("再次 Release，对象已销毁\n");

    CoUninitialize();
    printf("\n运行结束\n");
    return 0;
}

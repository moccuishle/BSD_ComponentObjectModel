/*
 * calciface.h - ICalc 接口定义（组件与客户端共享）
 *
 * 注意：接口方法必须按 QueryInterface / AddRef / Release 开头排列，
 * 这是 BSD-COM 的 ABI 约定，等同于 COM 的 IUnknown 前缀。
 */
#ifndef CALCIFACE_H
#define CALCIFACE_H

#include "bsdcom.h"

BSDCOM_INTERFACE(ICalc) {
    BSDCOM_HRESULT (*QueryInterface)(ICalc *, BSDCOM_REFIID, void **);
    BSDCOM_ULONG   (*AddRef)(ICalc *);
    BSDCOM_ULONG   (*Release)(ICalc *);
    BSDCOM_HRESULT (*Add)(ICalc *, int32_t a, int32_t b, int32_t *out);
    BSDCOM_HRESULT (*Mul)(ICalc *, int32_t a, int32_t b, int32_t *out);
};

extern const BSDCOM_IID BSDCOM_IID_ICalc;
extern const BSDCOM_CLSID BSDCOM_CLSID_Calc;

#endif /* CALCIFACE_H */

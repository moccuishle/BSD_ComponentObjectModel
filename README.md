# BSD-COM

一个面向 4.4BSD / BSD45 的 COM 风格组件运行时，从零实现，目标是在 Unix 上提供
`ole32` 最核心的三件事：**按 CLSID 激活对象、稳定二进制接口、引用计数生命周期**，
并自带一个 **IDL 编译器**（等价于 MIDL / widl），把接口定义变成代理与存根。

全部代码只依赖 POSIX（`dlopen` / `pthread` / 标准 C），不含任何微软代码，
依据的是 COM 的公开规范（IUnknown 三方法、类工厂、注册表、编组模型）与
公开的 MIDL 语法文档。

## 目录结构

```
bsd-com/
├── include/
│   ├── bsdcom.h              对外接口：类型、返回码、IUnknown、IClassFactory、GUID、激活 API
│   └── bsdcom_rpc.h          编组层：字节缓冲区、通道、存根派发签名
├── src/
│   ├── internal.h            运行时内部结构（类注册表条目）
│   ├── guid.c                GUID 解析/格式化/比较 + 预定义 IID
│   ├── registry.c            类注册表：注册、查找、注册脚本解析
│   ├── loader.c              进程内激活：dlopen + DllGetClassObject
│   ├── factory.c             通用类工厂（组件不必自己写 IClassFactory）
│   ├── coinit.c              CoInitialize / CoCreateInstance / CoGetClassObject
│   └── marshal.c             编组实现：缓冲区读写、通道、回环通道
├── tools/idlc/               ★ IDL 编译器（约 1700 行 C）
│   ├── idlc.h                token / AST / 接口数据结构
│   ├── lex.c                 词法分析
│   ├── parse.c               递归下降语法分析
│   ├── emit.c                代码生成（头文件 / GUID / 代理存根）
│   ├── util.c                诊断、类型映射
│   └── main.c                命令行驱动
├── examples/calc/            手写接口示例（不经编译器）
│   ├── calciface.h  guids.c  calc.c  client.c  bsdcom.reg
├── examples/calcgen/         经编译器生成的接口示例
│   ├── calc.idl              ★ 接口定义（唯一事实来源）
│   ├── gen/                  生成物：calc_idl.h / calc_i.c / calc_p.c
│   ├── impl.c                真实实现
│   └── demo.c                代理 → 编组 → 通道 → 存根 → 对象
├── tests/idl/edge.idl        编译器边界用例（void / 无参 / 浮点 / 双向参数 / 多接口）
└── Makefile
```

## IDL 编译器（idlc）

### 它做什么

```
calc.idl ──[lex.c]──> token 流 ──[parse.c]──> AST ──[emit.c]──> calc_idl.h  （vtable、IID、方法索引）
                                                              ├─> calc_i.c    （GUID、方法名表）
                                                              └─> calc_p.c    （代理 + 存根，含编组）
```

三个编译阶段都是手写的：词法分析逐字符扫描并跳过注释与预处理行，语法分析用递归下降
按 MIDL 语法子集构造接口/方法/参数的语法树，代码生成把树翻译成 C。

### 用法

```sh
./idlc -d calc.idl                  # 只解析，打印接口摘要
./idlc -o gen -v calc.idl           # 生成 gen/calc_{idl.h,i.c,p.c}
./idlc -h                           # 帮助
```

### 支持的语法

| 语法 | 状态 |
|---|---|
| `[uuid(...)] library Name { ... }` | 支持 |
| `[uuid(...)] interface IX : IUnknown { ... }` | 支持，仅支持直接派生 IUnknown |
| `[uuid(...)] coclass C { interface IX; }` | 支持，生成 `BSDCOM_CLSID_C` |
| `HRESULT` / `void` 返回 | 支持 |
| `[in] [out] [in,out] [retval]` | 支持 |
| `[string] const char *` 入参 | 支持（出参需调用方给缓冲区长度，暂不支持） |
| `long / short / char / unsigned / byte / boolean / float / double` | 支持 |
| 接口继承、联合体、`BSTR`、`VARIANT`、`size_is` 数组 | 明确报错，不静默猜 |

### 生成物长什么样

`calc.idl` 里写：

```idl
HRESULT Add([in] long a, [in] long b, [out, retval] long *result);
```

生成的 vtable 声明、代理和存根：

```c
/* calc_idl.h —— 接口声明 */
BSDCOM_INTERFACE(ICalc) {
    BSDCOM_HRESULT (*QueryInterface)(ICalc *, BSDCOM_REFIID, void **);
    BSDCOM_ULONG   (*AddRef)(ICalc *);
    BSDCOM_ULONG   (*Release)(ICalc *);
    BSDCOM_HRESULT (*Add)(ICalc *, int32_t a, int32_t b, int32_t *result);
};

/* calc_p.c —— 代理：参数变字节 */
static BSDCOM_HRESULT ICalc_Proxy_Add(ICalc *This, int32_t a, int32_t b, int32_t *result)
{
    ...
    BsdComBufferPutI32(&in, a);
    BsdComBufferPutI32(&in, b);
    hr = BsdComChannelInvoke(self->channel, 0, &in, &out);
    if (BSDCOM_SUCCEEDED(hr)) {
        BsdComBufferGetI32(&out, &v_result);
        if (result) *result = v_result;
    }
    ...
}

/* calc_p.c —— 存根：字节变参数，调用真实对象 */
static BSDCOM_HRESULT ICalc_Stub_Add(void *object, BSDCOM_BUFFER *in, BSDCOM_BUFFER *out)
{
    ICalc *iface = (ICalc *)object;
    int32_t a = 0, b = 0, v_result = 0;
    if (BsdComBufferGetI32(in, &a) < 0) return BSDCOM_E_INVALIDARG;
    if (BsdComBufferGetI32(in, &b) < 0) return BSDCOM_E_INVALIDARG;
    hr = iface->lpVtbl->Add(iface, a, b, &v_result);
    if (BSDCOM_SUCCEEDED(hr)) BsdComBufferPutI32(out, v_result);
    return hr;
}
```

### 线格式（wire format）

小端、紧凑、无对齐填充：定长标量按自然宽度原样写入，字符串是
`u32 长度(含 NUL) + 字节`，一次调用按参数声明顺序写完所有 `[in]` 参数；
返回码不走缓冲区，由通道返回值承载。

与 MIDL 的差别：MIDL 用 NDR（DCE/RPC 的线上表示，含对齐与类型描述符），
本实现用最简格式，只保证同机 / 同 ABI 互通。换成 socketpair 传输时不用改生成代码。

## 与 ole32 的对应关系

| ole32 / Windows | BSD-COM | 说明 |
|---|---|---|
| `CoInitialize` / `CoUninitialize` | `CoInitialize` / `CoUninitialize` | v1 单套间，无线程模型 |
| `CoCreateInstance` | `CoCreateInstance` | 取类工厂 → `CreateInstance` → 释放工厂 |
| `CoGetClassObject` | `CoGetClassObject` | 直接返回类工厂 |
| `IClassFactory` | `BSDCOM_IClassFactory` | 方法集一致，v1 不支持聚合 |
| `HKCR\CLSID\{...}\InprocServer32` | `/etc/bsdcom.reg` 文本注册表 | 无注册表数据库，纯文本、可版本化 |
| `LoadLibrary` + `GetProcAddress` | `dlopen` + `dlsym` | 同一套动态加载语义 |
| `DllGetClassObject` / `DllCanUnloadNow` | 同名导出符号 | 组件模块入口契约 |
| `IUnknown` | `BSDCOM_IUnknown` | 三方法在前，ABI 兼容 |
| `GUID` / `CLSID` / `IID` | `BSDCOM_GUID` 等 | 16 字节同布局，可与 Windows 侧互操作 |
| `midl.exe` | `idlc` | 输入 `.idl`，输出头文件 + `_i.c` + `_p.c` |
| `IMalloc` / `CoTaskMemAlloc` | 未实现 | 现在用 malloc/free |
| LRPC（跨进程编组） | 通道接口已就位 | 回环通道已验证编组；socketpair 传输待接 |

## 构建与运行

```sh
make            # idlc、libbsdcom.a、libcalc.so、calcclient、calcdemo
make check      # 激活流程 + IDL 生成的代理/存根往返
make test-idlc  # 编译器边界用例（生成并编译，不进最终产物）
```

### Windows 上的编译验证

没有 BSD 环境时，可用 MSYS2 UCRT64 的 gcc 验证代码正确性：

```sh
sh build-win.sh
```

`compat/win32/dlfcn.h` 会把 `dlopen/dlsym/dlclose` 映射到
`LoadLibraryA/GetProcAddress/FreeLibrary`。该垫片刻意不 include `<windows.h>`，
否则其 `objbase.h` 里真正的 `CoInitialize` / `CoCreateInstance` 会与项目同名 API 冲突。

已验证（gcc 16.2.0，`-Wall -Wextra` 全绿）：运行时 6 个编译单元、编译器 5 个编译单元、
IDL 生成物 3 份文件，均零警告。

```
===== 手写接口示例（CLSID 激活）=====
对象已创建: 000001965C7168F0
40 + 2 = 42
6 * 7  = 42
AddRef 后计数: 2
Release 一次后对象仍存活
再次 Release，对象已销毁

===== IDL 生成接口示例（代理/存根编组）=====
  方法索引: Add=0 Sub=1 Mul=2 Div=3 Lookup=4
  Add(40, 2)             = 42
  Sub(40, 2)             = 38
  Mul(6, 7)              = 42
  Div(84, 2)             = 42
  Div(1, 0)              预期失败，实际 HRESULT=0x80070057，出参保持 -12345
  Lookup("answer")       = 42
  Lookup("max")          = 2147483647
  QueryInterface(IUnknown)  HRESULT=0x00000000, 指针非空

通道统计: 7 次调用, 共 83 字节
最近一次请求的线格式:  8 字节:
    0000  04 00 00 00 6D 61 78 00
```

最后两行是编组的直接证据：`Lookup("max")` 的请求字节是
`04 00 00 00`（长度 4，含 NUL）+ `6D 61 78 00`（`"max"`）。

注册表查找顺序：`$BSDCOM_REGISTRY` → `/etc/bsdcom.reg` →
`/usr/local/etc/bsdcom.reg` → `./bsdcom.reg`。

## 组件开发三步

1. 写 `.idl`，用 `idlc` 生成 `_idl.h` / `_i.c` / `_p.c`；
2. 实现接口：把生成的 `<IFoo>Vtbl` 静态表挂在对象首个成员，导出
   `DllGetClassObject`（可直接调 `BsdComCreateFactory`）与 `DllCanUnloadNow`；
3. 在 `bsdcom.reg` 里登记 `{CLSID}  ./libfoo.so  描述`。

跨进程时，服务端在 channel 另一头调用 `<IFoo>_StubDispatch`，
客户端用 `<IFoo>_CreateProxy` 拿接口指针，中间换成什么传输都可以。

## 路线图

1. **已完成**：进程内激活、类注册表、通用类工厂、GUID、引用计数。
2. **已完成**：IDL 编译器 + 编组层（缓冲区 / 通道 / 回环）+ 代理存根生成。
3. **跨进程传输**：socketpair + 共享内存实现 LRPC，`BSDCOM_CLSCTX_LOCAL_SERVER` 打通。
   生成的代理存根不用改，只需实现一个 `Invoke` 走套接字的通道。
4. **套间与线程模型**：`CoInitializeEx` 的 STA/MTA 语义，每套间一个消息队列。
5. **结构化存储**：复合文档子集，对标 ole32 的 `IStorage` / `IStream`。
6. **自动化**：`IDispatch` 晚绑定 + 类型库（可由 idlc 顺带生成）。

## 已知限制

- 单套间（单线程模型），多线程同时激活需要外部加锁。
- 只支持 `BSDCOM_CLSCTX_INPROC_SERVER`，跨进程通道尚未接传输层。
- idlc 不支持接口继承、数组、`BSTR` / `VARIANT`、`[size_is]`；遇到会明确报错。
- 编组格式是本项目自定义的紧凑格式，不与 Windows NDR 互通（GUID 布局互通）。
- 不支持对象聚合（aggregation）、不支持 `CoFreeUnusedLibraries` 自动卸载。
- 错误码沿用 Windows HRESULT 数值，便于将来与 Wine 的行为做对照测试。

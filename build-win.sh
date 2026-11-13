#!/bin/sh
# build-win.sh - 在 Windows(MSYS2 UCRT64) 上做编译与行为验证
#
# BSD 目标平台请直接 `make`（用系统原生 dlfcn.h）。
# 本脚本只是为了让逻辑能在没有 BSD 环境的机器上跑起来做验证。
#
# 用法（在 MSYS2 UCRT64 shell 或 PATH 含 /c/msys64/ucrt64/bin 的 bash 中）：
#   sh build-win.sh
set -e

CC=${CC:-gcc}
CFLAGS="-O2 -g -Wall -Wextra -fPIC -Iinclude -Icompat/win32"
LIBS="-lpthread"
GEN=examples/calcgen/gen

mkdir -p build "$GEN"

# 1. IDL 编译器本身
$CC $CFLAGS -Itools/idlc -o idlc.exe \
    tools/idlc/main.c tools/idlc/lex.c tools/idlc/parse.c \
    tools/idlc/emit.c tools/idlc/util.c
echo "[1/6] idlc.exe"

# 2. 用编译器生成接口代码
./idlc.exe -o "$GEN" -v examples/calcgen/calc.idl
echo "[2/6] 生成 $GEN/calc_{idl.h,i.c,p.c}"

# 3. 运行时库
for f in src/*.c; do
    $CC $CFLAGS -c "$f" -o "build/$(basename "$f" .c).o"
done
rm -f libbsdcom.a
ar rcs libbsdcom.a build/guid.o build/registry.o build/loader.o \
    build/factory.o build/coinit.o build/marshal.o
echo "[3/6] libbsdcom.a"

# 4. 手写示例（examples/calc）+ 生成示例（examples/calcgen）
for f in examples/calc/calc.c examples/calc/guids.c examples/calc/client.c; do
    $CC $CFLAGS -Iexamples/calc -c "$f" -o "build/$(basename "$f" .c).o"
done
for f in "$GEN"/calc_i.c "$GEN"/calc_p.c examples/calcgen/impl.c examples/calcgen/demo.c; do
    $CC $CFLAGS -I"$GEN" -c "$f" -o "build/$(basename "$f" .c).o"
done
echo "[4/6] 目标文件"

rm -f libcalc.so calcclient.exe calcdemo.exe
$CC -shared -o libcalc.so build/calc.o build/guids.o libbsdcom.a $LIBS
$CC -o calcclient.exe build/client.o build/guids.o libbsdcom.a $LIBS
$CC -o calcdemo.exe build/impl.o build/demo.o build/calc_i.o build/calc_p.o \
    libbsdcom.a $LIBS
echo "[5/6] 链接完成"

# 6. 编译器边界用例：void 返回 / 无参数 / 浮点 / 双向参数 / 多接口
mkdir -p tests/idl/gen
./idlc.exe -o tests/idl/gen tests/idl/edge.idl
$CC $CFLAGS -Itests/idl/gen -c tests/idl/gen/edge_i.c -o build/edge_i.o
$CC $CFLAGS -Itests/idl/gen -c tests/idl/gen/edge_p.c -o build/edge_p.o
echo "[6/6] 边界用例生成并编译通过"

echo
echo "===== 手写接口示例（CLSID 激活）====="
BSDCOM_REGISTRY=examples/calc/bsdcom.reg ./calcclient.exe
echo
echo "===== IDL 生成接口示例（代理/存根编组）====="
./calcdemo.exe

# BSD-COM 构建脚本
# 目标平台：FreeBSD / NetBSD / OpenBSD 等 4.4BSD 衍生系统
# 在 Linux 上另需追加 -ldl（见 LIBS 注释）

CC      ?= cc
AR      ?= ar
CFLAGS  ?= -O2 -g -Wall
INCS    ?= -Iinclude
LIBS    ?= -pthread
# Linux 下改成：LIBS="-pthread -ldl"

IDLC_SRCS = tools/idlc/main.c tools/idlc/lex.c tools/idlc/parse.c \
            tools/idlc/emit.c tools/idlc/util.c

LIB_OBJS    = src/guid.o src/registry.o src/loader.o src/factory.o \
              src/coinit.o src/marshal.o

CALC_OBJS   = examples/calc/calc.o examples/calc/guids.o
CLIENT_OBJS = examples/calc/client.o examples/calc/guids.o

GEN_DIR     = examples/calcgen/gen
GEN_HDR     = $(GEN_DIR)/calc_idl.h
GEN_OBJS    = $(GEN_DIR)/calc_i.o $(GEN_DIR)/calc_p.o
DEMO_OBJS   = examples/calcgen/impl.o examples/calcgen/demo.o

EDGE_DIR    = tests/idl/gen
EDGE_HDR    = $(EDGE_DIR)/edge_idl.h

all: libbsdcom.a libcalc.so calcclient calcdemo

# ---------------------------------------------------------------- 编译器
idlc: $(IDLC_SRCS) tools/idlc/idlc.h
	$(CC) $(CFLAGS) -Itools/idlc -o $@ $(IDLC_SRCS)

# ---------------------------------------------------------------- 生成代码
$(GEN_HDR): examples/calcgen/calc.idl idlc
	@mkdir -p $(GEN_DIR)
	./idlc -o $(GEN_DIR) -v calc.idl

$(GEN_DIR)/calc_i.c $(GEN_DIR)/calc_p.c: $(GEN_HDR)

# ---------------------------------------------------------------- 目标文件
src/%.o: src/%.c include/bsdcom.h include/bsdcom_rpc.h src/internal.h
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

examples/calc/%.o: examples/calc/%.c include/bsdcom.h examples/calc/calciface.h
	$(CC) $(CFLAGS) $(INCS) -Iexamples/calc -c $< -o $@

$(GEN_DIR)/%.o: $(GEN_DIR)/%.c $(GEN_HDR) include/bsdcom.h include/bsdcom_rpc.h
	$(CC) $(CFLAGS) $(INCS) -I$(GEN_DIR) -c $< -o $@

examples/calcgen/%.o: examples/calcgen/%.c $(GEN_HDR) include/bsdcom.h include/bsdcom_rpc.h
	$(CC) $(CFLAGS) $(INCS) -I$(GEN_DIR) -c $< -o $@

# ---------------------------------------------------------------- 链接
libbsdcom.a: $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

libcalc.so: $(CALC_OBJS) libbsdcom.a
	$(CC) -shared -o $@ $(CALC_OBJS) libbsdcom.a $(LIBS)

calcclient: $(CLIENT_OBJS) libbsdcom.a
	$(CC) $(CFLAGS) $(INCS) -o $@ $(CLIENT_OBJS) libbsdcom.a $(LIBS)

calcdemo: $(DEMO_OBJS) $(GEN_OBJS) libbsdcom.a
	$(CC) $(CFLAGS) $(INCS) -I$(GEN_DIR) -o $@ \
	      $(DEMO_OBJS) $(GEN_OBJS) libbsdcom.a $(LIBS)

# ---------------------------------------------------------------- 编译器自测
$(EDGE_HDR): tests/idl/edge.idl idlc
	@mkdir -p $(EDGE_DIR)
	./idlc -o $(EDGE_DIR) tests/idl/edge.idl

$(EDGE_DIR)/%.o: $(EDGE_DIR)/%.c $(EDGE_HDR) include/bsdcom.h include/bsdcom_rpc.h
	$(CC) $(CFLAGS) $(INCS) -I$(EDGE_DIR) -c $< -o $@

# 边界用例：void 返回、无参数、float/double、[in,out]、多接口
test-idlc: idlc $(EDGE_DIR)/edge_i.o $(EDGE_DIR)/edge_p.o
	@echo "test-idlc: 边界用例生成并通过编译"

# ---------------------------------------------------------------- 验证
check: all test-idlc
	BSDCOM_REGISTRY=examples/calc/bsdcom.reg ./calcclient
	@echo
	./calcdemo

install: all
	mkdir -p $(PREFIX)/lib $(PREFIX)/include $(PREFIX)/bin
	cp libbsdcom.a $(PREFIX)/lib/
	cp include/bsdcom.h include/bsdcom_rpc.h $(PREFIX)/include/
	cp idlc $(PREFIX)/bin/

PREFIX ?= /usr/local

clean:
	rm -f $(LIB_OBJS) $(CALC_OBJS) $(CLIENT_OBJS) $(DEMO_OBJS) $(GEN_OBJS) \
	      libbsdcom.a libcalc.so calcclient calcdemo idlc
	rm -rf $(GEN_DIR) $(EDGE_DIR)

.PHONY: all check test-idlc install clean

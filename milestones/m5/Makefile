CC=/home/tim/build-llvm/bin/clang
LD=/home/tim/build-llvm/bin/wasm-ld
V8=/home/tim/v8/out/x64.optdebug/d8
JSC?=jsc
SPIDERMONKEY?=js

all: test.wasm # jsc.test spidermonkey.test

v8.test: JS?=$(V8) --expose-gc 
jsc.test: JS?=$(JSC) --useWeakRefs=true
spidermonkey.test: JS?=$(SPIDERMONKEY)

.PHONY: %.test
%.test: test.js ../lib.js test.wasm
	$(JS) $<

%.o: %.c
	$(CC) -Oz --target=wasm32 -mreference-types -nostdlib -c -o $@ $<

test.wasm: test.o walloc.o
	$(LD) --no-entry --import-memory -o $@ $^

.PHONY: clean
clean:
	rm -f *.o test.wasm

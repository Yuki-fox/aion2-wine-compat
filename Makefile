CC = clang
CFLAGS = -arch x86_64 -O2 -Wall -Wextra -Werror -fno-stack-protector

.PHONY: all test

all: build/compat_cpuid.dylib

build:
	mkdir -p build

build/compat_cpuid.dylib: compat_cpuid.c | build
	$(CC) $(CFLAGS) -dynamiclib -Wl,-install_name,@rpath/compat_cpuid.dylib $< -o $@

build/compat_test: tests/compat_test.c compat_cpuid.c Makefile | build
	$(CC) $(CFLAGS) -Wl,-pagezero_size,0x1000 $< -o $@

test: build/compat_test
	./build/compat_test

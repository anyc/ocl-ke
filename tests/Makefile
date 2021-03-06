
LDLIBS+=-lOpenCL
CFLAGS+=-g

TESTS:=$(wildcard *.c) add_ocl10.c add_ocl12.c add_lib.c
KERNELS:=$(filter-out %.inc.cl,$(wildcard *.cl))

.PHONY: clean

%: %.c

%.bin: %.cl
	../ocl-ke $<

%.ocl10.bin: %.inc.cl
	../ocl-ke $< -b "-I$(shell pwd)" -o $@

%.ocl12.bin: %.inc.cl %.h.cl
	../ocl-ke $< -i $*.h.cl -o $@

library.bin: add_kernel.ocl12.bin increment_kernel.bin
	../ocl-ke -I add_kernel.ocl12.bin -I increment_kernel.bin -s -o $@

%.test: %
	@echo Running $<...
	@./$<

all: $(TESTS:.c=) $(KERNELS:.cl=.bin)

add_ocl10.o: CFLAGS+=-DKERNEL_NAME=add -DKERNEL_FILE=add_kernel.ocl10.bin
add_ocl10.o: increment.c add_kernel.ocl10.bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

add_ocl12.o: CFLAGS+=-DKERNEL_NAME=add -DKERNEL_FILE=add_kernel.ocl12.bin
add_ocl12.o: increment.c add_kernel.ocl12.bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

add_lib.o: CFLAGS+=-DKERNEL_NAME=add -DKERNEL_FILE=library.bin
add_lib.o: increment.c library.bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TESTS:.c=) *.o *.bin

check: $(TESTS:.c=.test)
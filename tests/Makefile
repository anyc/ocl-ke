
LDLIBS+=-lOpenCL
CFLAGS+=-g

TESTS:=$(wildcard *.c) add_ocl10.c
KERNELS:=$(filter-out %.inc.cl,$(wildcard *.cl))

.PHONY: clean

%: %.c

%.bin: %.cl
	../ocl-ke $<

%.ocl10.bin: %.inc.cl
	../ocl-ke $< -b "-I$(shell pwd)" -o $@

%.test: %
	@echo Running $<...
	@./$<

all: $(TESTS:.c=) $(KERNELS:.cl=.bin)

add_ocl10.o: CFLAGS+=-DKERNEL_NAME=add -DKERNEL_FILE=add_kernel.ocl10.bin
add_ocl10.o: increment.c add_kernel.ocl10.bin
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TESTS:.c=) *.o *.bin

check: $(TESTS:.c=.test)
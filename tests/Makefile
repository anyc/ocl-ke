
LDLIBS+=-lOpenCL
CFLAGS+=-g

TESTS:=$(wildcard *.c)
KERNELS:=$(wildcard *.cl)

.PHONY: clean %.tested

%: %.o

%.bin: %.cl
	../ocl-ke $<

%.test: %
	@echo Running $<...
	@./$<

all: $(TESTS:.c=) $(KERNELS:.cl=.bin)

clean:
	rm -f $(TESTS:.c=)

run: $(TESTS:.c=.test)
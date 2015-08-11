
APP=ocl-ke

LDLIBS+=-lOpenCL

### enable dynamic OpenCL v1.2 detection
LDLIBS+=-ldl
CFLAGS+=-DOCL_AUTODETECT

.PHONY: clean

all: $(APP)

$(APP): $(APP).o

debug: CFLAGS += -g
debug: LDFLAGS += -g
debug: $(APP)

clean:
	rm -f $(APP) $(APP).o
	$(MAKE) -C tests/ clean

test:
	$(MAKE) -C tests/

check:
	$(MAKE) -C tests/ check
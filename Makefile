
APP=ocl-ke

LDLIBS+=-lOpenCL

.PHONY: clean

all: $(APP)

$(APP): $(APP).o

debug: CFLAGS += -g
debug: LDFLAGS += -g
debug: $(APP)

clean:
	rm -f $(APP)
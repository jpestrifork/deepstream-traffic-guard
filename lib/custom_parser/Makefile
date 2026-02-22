CC:= g++

CUDA_VER:=12.2

CFLAGS:= -Wall -Werror -std=c++11 -shared -fPIC -Wno-error=deprecated-declarations

CFLAGS+= -I/opt/nvidia/deepstream/deepstream-7.0/sources/includes \
	-I /usr/local/cuda-$(CUDA_VER)/include

LIBS:= -lnvinfer -lnvparsers
LFLAGS:= -Wl,--start-group $(LIBS) -Wl,--end-group

SRCFILES:= nvinfer_custom_lpr_parser.cpp
TARGET_LIB:= libnvdsinfer_custom_impl_lpr.so

all: $(TARGET_LIB)

$(TARGET_LIB) : $(SRCFILES)
	$(CC) -o $@ $^ $(CFLAGS) $(LFLAGS)

clean:
	rm -rf $(TARGET_LIB)
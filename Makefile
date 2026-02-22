CUDA_VER     ?= 12.2
NVDS_VERSION ?= 7.0

APP     := traffic-guard
BINDIR  := bin

TARGET_DEVICE := $(shell gcc -dumpmachine | cut -f1 -d -)
ifeq ($(TARGET_DEVICE),aarch64)
  PLATFORM_FLAGS := -DPLATFORM_TEGRA
endif

LIB_INSTALL_DIR ?= /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/
NVDS_INCLUDE     := /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/sources/includes
CUDA_INCLUDE     := /usr/local/cuda-$(CUDA_VER)/include

CC      := gcc
PKGS    := gstreamer-1.0
CFLAGS  := -Wall -Wextra -g \
           $(PLATFORM_FLAGS) \
           -I include \
           -I $(NVDS_INCLUDE) \
           -I $(CUDA_INCLUDE) \
           $(shell pkg-config --cflags $(PKGS))

LIBS    := $(shell pkg-config --libs $(PKGS))
LIBS    += -L/usr/local/cuda-$(CUDA_VER)/lib64/ -lcudart -lm \
           -L$(LIB_INSTALL_DIR) -lnvdsgst_meta -lnvds_meta -lnvds_yml_parser \
           -Wl,-rpath,$(LIB_INSTALL_DIR)

SRCDIR  := src
BUILDDIR:= build

SRCS    := $(SRCDIR)/main.c \
           $(SRCDIR)/config.c \
           $(SRCDIR)/logger.c \
           $(SRCDIR)/pipeline_builder.c \
           $(SRCDIR)/pipeline_linker.c \
           $(SRCDIR)/probe_base.c \
           $(SRCDIR)/probes/probe_detections.c \
           $(SRCDIR)/probes/probe_tracker_match.c \
           $(SRCDIR)/probes/probe_send.c \
           $(SRCDIR)/probes/probe_drop.c \
           $(SRCDIR)/director.c \
           $(SRCDIR)/pipeline_controller.c
OBJS    := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all custom_parser clean

all: $(BINDIR)/$(APP)

$(BINDIR)/$(APP): $(OBJS) | $(BINDIR)
	$(CC) -g -o $@ $(OBJS) $(LIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINDIR):
	mkdir -p $(BINDIR)

custom_parser:
	$(MAKE) -C lib/custom_parser

clean:
	rm -rf $(BUILDDIR) $(BINDIR)/$(APP)
	$(MAKE) -C lib/custom_parser clean

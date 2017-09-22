# use pkg-config for getting CFLAGS and LDLIBS
FFMPEG_LIBS=    libavformat                        \
                libavutil                          \
                libswresample					   \

PKG_CFG_LIBS=   x11                                \
                vdpau                              \
                libva                              \
                $(FFMPEG_LIBS)


CFLAGS += -Wall -O2 -g
CFLAGS := $(shell pkg-config --cflags $(PKG_CFG_LIBS)) $(CFLAGS)

LDLIBS := $(shell pkg-config --libs $(PKG_CFG_LIBS)) $(LDLIBS)
LDLIBS += -lvdpau -lX11 -lva -lva-x11  -lva-drm -lm
LDLIBS += -lssl -lcrypto

INC += -I../extension/ffbuild/include -I../extension/rtmp/ -I../extension/resample/ -I../extension/vic/
INC += -I../extension/transcode/include -I../extension/transcode/include/fdk-aac  

LDFLAGS = -L../../../Lib/Linux/$(ARCH) -lstdc++ 
LDFLAGS += -L../extension/transcode/lib -lfdk-aac -lm

#LDFLAGS += -L'./extension/vic/lib' -lvUtil
EXT_MODULES := ../extension/vic/kqueue.c
EXT_MODULES += ../extension/resample/KFResample.c
EXT_MODULES += ../extension/rtmp/MKRTMP.c
EXT_MODULES += ../extension/rtmp/nake_srs/srs_librtmp.a
EXT_MODULES += ../extension/transcode/aactranscode.c

all: $(EXT_MODULES)
	@echo "=== Target Platform [$(ARCH)] ==="
	gcc $(CFLAGS) mp4gortmp.c $(EXT_MODULES) $(INC) $(LDFLAGS) $(LDLIBS) -g -o mp4gortmp.x

clean:
	rm -rf *.o *~ *.bak *.x tags
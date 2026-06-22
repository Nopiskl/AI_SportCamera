CC:=~/workspaces/tina-v853-open/prebuilt/rootfsbuilt/arm/toolchain-sunxi-musl-gcc-830/toolchain/bin/arm-openwrt-linux-gcc
CXX:=~/workspaces/tina-v853-open/prebuilt/rootfsbuilt/arm/toolchain-sunxi-musl-gcc-830/toolchain/bin/arm-openwrt-linux-g++

INCLUDES += -I /home/book/workspaces/tina-v854-open/out/v853/100ask/openwrt/staging_dir/target/usr/include/
INCLUDES += -I /home/book/workspaces/tina-v853-open/out/v853/100ask/openwrt/staging_dir/target/usr/include/opencv4
INCLUDES += -I /home/book/workspaces/tina-v853-open/out/v853/100ask/openwrt/staging_dir/target/usr/include/opencv4/opencv2
INCLUDES += -I $(shell pwd)/include -I $(shell pwd)/isp/ -I $(shell pwd)/isp/include -I $(shell pwd)/isp/include/device

LIB_PATH := /home/book/workspaces/tina-v853-open/out/v853/100ask/openwrt/staging_dir/target/usr/lib/
LIBS += -L$(LIB_PATH) -L /home/book/workspaces/tina-v853-open/prebuilt/rootfsbuilt/arm/toolchain-sunxi-musl-gcc-830/toolchain/lib/ -L$(shell pwd)/isp/lib -L$(shell pwd)/lib/

LIBS += -ljpeg -lVIPlite -lVIPuser -lstdc++ -lISP -lisp_af -lisp_awb -lisp_gtm -lisp_pltm -lisp_ae -lisp_afs -lisp_base -lisp_iso -lisp_ini -lisp_dev -lisp_math

CFLAGS =-Wl,-rpath-link=$(LIB_PATH)

BIN=yolov5-camera

SRCS=${wildcard *.c}
SRCS += ${wildcard *.cpp}
OBJS=$(addsuffix .o, $(basename $(SRCS)))

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ -lm

.cpp.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c $<

.cpp:
	$(CXX) $(CFLAGS) $(INCLUDES) $< -o $@ -lm

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) $(EXTRALFLAGS) $(OBJS) $(LIBS) -I/home/book/workspaces/tina-v853-open/out/v853/100ask/openwrt/staging_dir/target/usr/include/ -g -o $@ -l VIPlite \
                -lopencv_objdetect -lopencv_calib3d -lopencv_features2d -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs \
                -lopencv_video -lopencv_photo -lopencv_ml -lopencv_imgproc -lopencv_flann -lopencv_core


clean:
	rm -rf *.o
	rm -rf $(BIN)
	rm -rf *~


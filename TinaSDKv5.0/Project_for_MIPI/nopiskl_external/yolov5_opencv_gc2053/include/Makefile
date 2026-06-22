CC:=arm-openwrt-linux-gcc

INCLUDES += -I/home/ubuntu/Project/yolov3/yolov3/include
LIB_PATH := /home/ubuntu/Project/yolov3/yolov3/lib
LIBS += -L$(LIB_PATH)

LIBS += -ljpeg

BIN=yolov3

SRCS=${wildcard *.c}
OBJS=$(addsuffix .o, $(basename $(SRCS)))

.c.o:
	$(CC) $(CFLAGS) -c $<

.c:
	$(CC) $(CFLAGS) $< -o $@ -lm

all: $(BIN) 

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) $(EXTRALFLAGS) $(OBJS) $(LIBS) -g -o $@ -l VIPlite 

clean:
	rm -rf *.o
	rm -rf $(BIN)
	rm -rf *~

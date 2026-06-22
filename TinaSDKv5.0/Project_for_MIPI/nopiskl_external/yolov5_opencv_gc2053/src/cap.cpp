/*============================================================================
w 打开只写文件，若文件存在则文件长度清为0，即该文件内容会消失。若文件不存在则建立该文件。
w+ 打开可读写文件，若文件存在则文件长度清为零，即该文件内容会消失。若文件不存在则建立该文件。
a 以附加的方式打开只写文件。若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到文件尾，即文件原先的内容会被保留。（EOF符保留）
a+ 以附加方式打开可读写的文件。若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到文件尾后，即文件原先的内容会被保留。 （原来的EOF符不保留)

在进行V4L2开发中，常用的命令标志符如下(some are optional)：
?    VIDIOC_REQBUFS：分配内存
?    VIDIOC_QUERYBUF：把VIDIOC_REQBUFS中分配的数据缓存转换成物理地址
?    VIDIOC_QUERYCAP：查询驱动功能
?    VIDIOC_ENUM_FMT：获取当前驱动支持的视频格式
?    VIDIOC_S_FMT：设置当前驱动的频捕获格式
?    VIDIOC_G_FMT：读取当前驱动的频捕获格式
?    VIDIOC_TRY_FMT：验证当前驱动的显示格式
?    VIDIOC_CROPCAP：查询驱动的修剪能力
?    VIDIOC_S_CROP：设置视频信号的边框
?    VIDIOC_G_CROP：读取视频信号的边框
?    VIDIOC_QBUF：把数据从缓存中读取出来
?    VIDIOC_DQBUF：把数据放回缓存队列
?    VIDIOC_STREAMON：开始视频显示函数
?    VIDIOC_STREAMOFF：结束视频显示函数
?    VIDIOC_QUERYSTD：检查当前视频设备支持的标准，例如PAL或NTSC。

=============================================================================*/
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>


#define LOG_TAG    "AWIspApi"

#ifdef __cplusplus
extern "C" {
#endif

#include "device/isp_dev.h"
#include "tools.h"

#include "events.h"
#include "isp_tuning_priv.h"
#include "isp_manage.h"

#include "iniparser.h"

#include "isp.h"

#ifdef __cplusplus
}
#endif

#include <linux/videodev2.h>


#define FILE_VIDEO  "/dev/video0"
#define JPG       "./image%d.yuv"

#define WIDTH  1920
#define HEIGHT 1088

#define FMT_NUM_PLANES 1

enum io_method {
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
    IO_METHOD_DMABUF,
};


typedef struct{
    void *start;
    int length;
}   BUFTYPE;
BUFTYPE *usr_buf;

struct buffer {
    void *start;
    size_t length;
    struct v4l2_buffer v4l2_buf;
};

static int io = IO_METHOD_MMAP;
static enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

static unsigned int n_buffer = 0;

//set video capture ways(mmap)
int init_mmap(int fd)
{
    //to request frame cache, contain requested counts
    struct v4l2_requestbuffers reqbufs;
    //request V4L2 driver allocation video cache
    //this cache is locate in kernel and need mmap mapping
    memset(&reqbufs, 0, sizeof(reqbufs));
    reqbufs.count = 3;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if(-1 == ioctl(fd,VIDIOC_REQBUFS,&reqbufs)){
        perror("Fail to ioctl 'VIDIOC_REQBUFS'");
        exit(EXIT_FAILURE);
    }
    n_buffer = reqbufs.count;
    printf("n_buffer = %d\n", n_buffer);
    usr_buf = (BUFTYPE*)calloc(reqbufs.count, sizeof(BUFTYPE));
    if(usr_buf == NULL){
        printf("Out of memory\n");
        exit(-1);
    }
    //map kernel cache to user process
    for(n_buffer = 0; n_buffer < reqbufs.count; ++n_buffer){
        //stand for a frame
        struct v4l2_buffer buf;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffer;


        if(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf.type){
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }
        //check the information of the kernel cache requested检查所请求的内核缓存的信息
        if(-1 == ioctl(fd,VIDIOC_QUERYBUF,&buf))
        {
            perror("Fail to ioctl : VIDIOC_QUERYBUF");
            exit(EXIT_FAILURE);
        }


        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ==  buf.type) {
               usr_buf[n_buffer].length = buf.m.planes[0].length;
               usr_buf[n_buffer].start =
                   mmap(NULL /* start anywhere */,
                         buf.m.planes[0].length,
                         PROT_READ | PROT_WRITE /* required */,
                         MAP_SHARED /* recommended */,
                         fd, buf.m.planes[0].m.mem_offset);
       } else {
           usr_buf[n_buffer].length = buf.length;
           usr_buf[n_buffer].start =
               mmap(NULL /* start anywhere 开始的地方*/,
                     buf.length,
                     PROT_READ | PROT_WRITE /* required */,
                     MAP_SHARED /* recommended */,
                     fd, buf.m.offset);
       }


        printf("usr_buf[%d].start=0x%x\n",n_buffer,usr_buf[n_buffer].start);

        if(MAP_FAILED == usr_buf[n_buffer].start)
        {
            perror("Fail to mmap");
            exit(EXIT_FAILURE);
        }

        printf("usr_buf %d: address=0x%x, length=%d\n", n_buffer, (unsigned int)usr_buf[n_buffer].start, usr_buf[n_buffer].length);

    }
    return 0;
}

//initial camera device
int init_camera_device(int fd)
{
    //decive fuction, such as video input
    struct v4l2_capability cap;
    //video standard,such as PAL,NTSC
    struct v4l2_standard std;
    //frame format
    struct v4l2_format tv_fmt;
    //check control
    struct v4l2_queryctrl query;
    //detail control value
    struct v4l2_fmtdesc fmt;
    int ret;
    //get the format of video supply  //获取视频格式
    memset(&fmt, 0, sizeof(fmt));
    fmt.index = 0;
    //supply to image capture  //提供图像采集
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    // show all format of supply  显示所有格式的供应
    printf("Support format:\n");
    /* 3.select the current video input 选择当前的视频输入*/
    struct v4l2_input inp;           /* select the current video input选择当前的视频输入*/
    memset(&inp, 0, sizeof(inp));
    inp.index = 0;
    inp.type = V4L2_INPUT_TYPE_CAMERA;
    if (ioctl(fd, VIDIOC_S_INPUT, &inp) < 0) {
         printf(" VIDIOC_S_INPUT failed! s_input: %d\n", inp.index);
        close(fd); //关闭文件
        return -1;
    }
    //VIDIOC_ENUM_FMT用于枚举视频设备支持的视频格式
    while(ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0)
    {
        fmt.index++;
        printf("pixelformat = ''%c%c%c%c''\ndescription = ''%s''\n",fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,(fmt.pixelformat >> 16) & 0xFF,                   (fmt.pixelformat    >> 24) & 0xFF,fmt.description);
    }
    //check video decive driver capability  ，//用于查询视频设备的能力
    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if(ret < 0){
        perror("Fail to ioctl VIDEO_QUERYCAP");
        exit(EXIT_FAILURE);
    }
    // Print capability infomations
    printf("Capability Informations:\n");
    printf(" driver: %s\n", cap.driver);
    printf(" card: %s\n", cap.card);
    printf(" bus_info: %s\n", cap.bus_info);
    printf(" version: %08X\n", cap.version);
    printf(" capabilities: %08X\n", cap.capabilities);
    printf("----------------------------\n");
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
            !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        printf("The Current device is not a video capture device, capabilities: %x\n", cap.capabilities);
            exit(EXIT_FAILURE);
    }

    //judge whether or not to supply the form of video stream  判断是否提供视频流的格式
    if(!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        printf("The Current device does not support streaming i/o\n");
        exit(EXIT_FAILURE);
    }

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;



    //set the form of camera capture data 设置相机捕捉数据的格式
    memset(&fmt, 0, sizeof(   struct v4l2_fmtdesc));    // very important!must clear 0
    tv_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    tv_fmt.fmt.pix.width = WIDTH;     //1280;    //680;  //1920
    tv_fmt.fmt.pix.height = HEIGHT;    //720;    //480;  //1080
    tv_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21; //V4L2_PIX_FMT_YUYV;
    tv_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

//    tv_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; //摄像头采集
//    tv_fmt.fmt.pix.width = 1920; //设置摄像头采集参数，不可以任意设置
//    tv_fmt.fmt.pix.height = 1088;
//    tv_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;

    if (ioctl(fd, VIDIOC_S_FMT, &tv_fmt)< 0) {
        printf("VIDIOC_S_FMT FAIL!\n");
        exit(-1);
        close(fd);
    }



    if(ioctl(fd, VIDIOC_G_FMT, &tv_fmt) < 0)
    {
        printf("VIDIOC_G_FMT FAIL!\n");
        exit(-1);
        close(fd);

    }

    // Print Stream Format  打印流格式
    printf("Stream Format Informations:\n");
    printf(" type: %d\n", tv_fmt.type);
    printf(" width: %d\n", tv_fmt.fmt.pix.width);
    printf(" height: %d\n", tv_fmt.fmt.pix.height);
    char fmtstr[8];
    memset(fmtstr, 0, 8);
    memcpy(fmtstr, &tv_fmt.fmt.pix.pixelformat, 4);
    printf(" pixelformat: %s\n", fmtstr);
    printf(" field: %d\n", tv_fmt.fmt.pix.field);
    printf(" bytesperline: %d\n", tv_fmt.fmt.pix.bytesperline);
    printf(" sizeimage: %d\n", tv_fmt.fmt.pix.sizeimage);
    printf(" colorspace: %d\n", tv_fmt.fmt.pix.colorspace);
    printf(" priv: %d\n", tv_fmt.fmt.pix.priv);
    printf(" raw_date: %s\n", tv_fmt.fmt.raw_data);



    //initial video capture way(mmap)  初始视频捕获方式(mmap)
    init_mmap(fd);


    return 0;
}

int open_camera_device()
{
    int fd;
    //open video device with block //用块打开视频设备
    fd = open(FILE_VIDEO, O_RDWR | O_NONBLOCK, 0);
    if(fd < 0){
        perror(FILE_VIDEO);
        exit(EXIT_FAILURE);
    }
    return fd;
}

int start_capture(int fd)
{
    unsigned int i;
    enum v4l2_buf_type type;

    //place the kernel cache to a queue将内核缓存放置到一个队列
    for(i = 0; i < n_buffer; i++){
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = buf_type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf.type) {
            struct v4l2_plane planes[FMT_NUM_PLANES];

            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        if(-1 == ioctl(fd, VIDIOC_QBUF, &buf)){
            perror("Fail to ioctl 'VIDIOC_QBUF'");
            exit(EXIT_FAILURE);
        }
    }

    //start capture data
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if(-1 == ioctl(fd, VIDIOC_STREAMON, &type)){
        printf("i=%d.\n", i);
        perror("VIDIOC_STREAMON");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return 0;
}




int process_image_video(void *addr, int length)
{
    FILE *fp;

    static int num = 0;

    char image_name[32];
    sprintf(image_name, "/home/v4l2_yuv_video%d.yuv", num++);
    if (num>7)
        {
            if((fp = fopen(image_name, "w")) == NULL)
            {
                printf("Fail to fopen");
                exit(EXIT_FAILURE);
            }

            fwrite(addr, length, 1, fp);
            usleep(500);
            fclose(fp);
        }

    return 0;
}


int read_frame(int fd)
{
    struct v4l2_buffer buf;
    unsigned int i, bytesused;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ==  buf.type) {
        struct v4l2_plane planes[FMT_NUM_PLANES];
        buf.m.planes = planes;
        buf.length = FMT_NUM_PLANES;
    }

    //put cache from queue将缓存从队列中移除
    if(-1 == ioctl(fd, VIDIOC_DQBUF,&buf)){
        perror("Fail to ioctl 'VIDIOC_DQBUF'");
        exit(EXIT_FAILURE);
    }

    assert(buf.index < n_buffer);
    printf("buf.index=%d,n_buffer=%d\n",buf.index,n_buffer);
    printf(" usr_buf[%d].start=0x%x,usr_buf[%d].length=0x%x\n", buf.index,usr_buf[buf.index].start,buf.index,usr_buf[buf.index].length);


    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf.type)
        bytesused = buf.m.planes[0].bytesused;
    else
        bytesused = buf.bytesused;

     printf("read_frame bytesused=%d\n",bytesused);
    //read process space's data to a file //读取进程空间的数据到一个文件
    process_image_video(usr_buf[buf.index].start, bytesused);

    if(-1 == ioctl(fd, VIDIOC_QBUF,&buf))
    {
        perror("Fail to ioctl 'VIDIOC_QBUF'");
        exit(EXIT_FAILURE);
    }

    return 1;
}





int mainloop(int fd)
{
    int count =8;

    while(count-- > 0)
    {
        for(;;)
        {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO(&fds);
            FD_SET(fd,&fds);

            /*Timeout*/
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            r = select(fd + 1,&fds,NULL,NULL,&tv);

            if(-1 == r)
            {
                if(EINTR == errno)
                    continue;
                perror("Fail to select");
                //exit(EXIT_FAILURE);
            }

            if(0 == r)
            {
                fprintf(stderr,"select Timeout\n");
                exit(-1);
            }

            if(read_frame(fd))
                break;
        }
    }
    return 0;
}

void stop_capture(int fd)
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if(-1 == ioctl(fd,VIDIOC_STREAMOFF,&type))
    {
        perror("Fail to ioctl 'VIDIOC_STREAMOFF'");
        exit(EXIT_FAILURE);
    }
    return;
}

void close_camera_device(int fd)
{
    unsigned int i;
    for(i = 0;i < n_buffer; i++)
    {
        if(-1 == munmap(usr_buf[i].start,usr_buf[i].length)){
            exit(-1);
        }
    }
    free(usr_buf);

    if(-1 == close(fd))
    {
        perror("Fail to close fd");
        exit(EXIT_FAILURE);
    }
    return;
}

int fd;

int main()
{
    int ret;
    fd = open_camera_device();
    printf("-------open_camera_device-------\n");
    init_camera_device(fd);
//    ret = isp_init(0);
//    ret = isp_run(0);
    //usleep(1000000); //usleep(10000); //单位是微秒 1000000us = 1s
//    ret = isp_stop(0);
    printf("-------init_camera_device-------\n");
//    ret = isp_init(0);
//    ret = isp_run(0);
    start_capture(fd);
    printf("-------start_capture-------\n");

    mainloop(fd);
    printf("-------mainloop-------\n");

    stop_capture(fd);
    printf("-------stop_capture-------\n");

    close_camera_device(fd);
//    ret = isp_stop(0);
//    ret = isp_pthread_join(0);
//    ret = isp_exit(0);
    printf("-------close_camera_device-------\n");

    return 0;
}



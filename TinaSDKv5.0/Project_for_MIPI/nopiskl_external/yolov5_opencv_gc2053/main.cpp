
#include <iostream>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include <linux/fb.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/fb.h>
#include <stdint.h>
#include <sys/ioctl.h>



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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vip_lite.h>

#define _BASETSD_H
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/types_c.h>  

extern "C" {
#include "vnn_global.h"


#include "device/isp_dev.h"
#include "isp_tuning_priv.h"
#include "isp_manage.h"
#include "isp.h"
#include "isp/include/isp.h"
}

using namespace std;

enum Yolov5OutType
{
    p8_type     = 1,
    p16_type    = 2,
    p32_type    = 3,
};

struct Object
{
    cv::Rect_<float> rect;
    int label;
    float prob;
};

//typedef struct blob
//{
//    int w;
//    float *data;
//} blob;
//cv::Mat data1=cv::imread("416_416bus.jpg");
//cv::Mat data1=cv::imread("640bus.jpg");
int hw=320;
cv::Mat data1;

static inline float sigmoid(float x)
{
    return static_cast<float>(1.f / (1.f + exp(-x)));
}
/*
这是一个计算两个矩形框相交面积的函数，其中 Object 是一个自定义的类型，表示一个物体检测框，包含了矩形框的位置、大小等信息。cv::Rect_<float> 则是OpenCV中用于表示矩形框的类型，它包含了矩形框的左上角坐标和宽度、高度等信息。

具体来说，该函数的实现过程如下：

首先，通过 a.rect & b.rect 计算两个矩形框的交集，得到一个新的矩形框 inter。
然后，通过 inter.area() 计算矩形框 inter 的面积，即为两个矩形框的相交面积。
最后，将相交面积作为函数的返回值。
需要注意的是，该函数使用了 static inline 修饰符，表示这是一个静态内联函数，可以在编译时直接将函数调用展开为函数体，以提高程序的执行效率。
 */
static inline float intersection_area(const Object& a, const Object& b)
{
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}
/*
这是一个使用快速排序算法对物体检测结果进行降序排序的函数，其中 Object 是一个自定义的类型，表示一个物体检测框，包含了矩形框的位置、大小、置信度等信息。std::vector<Object> 则是一个存储物体检测结果的向量容器。
具体来说，该函数的实现过程如下：
首先，通过 left 和 right 参数指定待排序的物体检测结果的范围。
然后，选取中间位置的物体检测结果的置信度 p 作为枢轴元素。
接着，从左边开始搜索，找到第一个置信度小于等于 p 的物体检测结果，将其位置记为 i。
从右边开始搜索，找到第一个置信度大于等于 p 的物体检测结果，将其位置记为 j。
如果 i 小于等于 j，则交换 faceobjects[i] 和 faceobjects[j]，并将 i 和 j 分别加一和减一。
重复步骤 3~5，直到 i 大于 j。
如果 left 小于 j，则递归调用 qsort_descent_inplace 函数对左半部分进行排序。
如果 i 小于 right，则递归调用 qsort_descent_inplace 函数对右半部分进行排序。
需要注意的是，该函数使用了 static 修饰符，表示这是一个静态函数，可以在不创建对象的情况下直接调用。此外，该函数是一个 void 类型函数，即没有返回值，而是直接对输入的向量容器进行了排序
*/
static void qsort_descent_inplace(std::vector<Object>& faceobjects, int left, int right)
{
    int i = left;
    int j = right;
    float p = faceobjects[(left + right) / 2].prob;

    while (i <= j)
    {
        while (faceobjects[i].prob > p)
            i++;

        while (faceobjects[j].prob < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(faceobjects[i], faceobjects[j]);

            i++;
            j--;
        }
    }

#pragma omp parallel sections
    {
#pragma omp section
        {
            if (left < j) qsort_descent_inplace(faceobjects, left, j);
        }
#pragma omp section
        {
            if (i < right) qsort_descent_inplace(faceobjects, i, right);
        }
    }
}

static void qsort_descent_inplace(std::vector<Object>& faceobjects)
{
    if (faceobjects.empty())
        return;

    qsort_descent_inplace(faceobjects, 0, faceobjects.size() - 1);
}
//这是一个使用非极大值抑制（NMS）算法对物体检测结果进行筛选的函数
static void nms_sorted_bboxes(const std::vector<Object>& faceobjects, std::vector<int>& picked, float nms_threshold)
{
    picked.clear();

    const int n = faceobjects.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        areas[i] = faceobjects[i].rect.area();
    }

    for (int i = 0; i < n; i++)
    {
        const Object& a = faceobjects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            const Object& b = faceobjects[picked[j]];

            // intersection over union//交并集
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            // float IoU = inter_area / union_area
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep)
            picked.push_back(i);
    }
}
/*
这是一个生成物体检测框的函数，它使用了基于锚点的物体检测算法，
其中 stride 是特征图的步长，feat 是特征图的指针，prob_threshold 是置信度阈值，
objects 是一个存储物体检测结果的向量容器，letterbox_cols 和 letterbox_rows 是图像的宽度和高度，用于计算检测框的位置。

*/
static void generate_proposals(int stride, const float* feat, float prob_threshold, std::vector<Object>& objects,
                               int letterbox_cols, int letterbox_rows)
{
    static float anchors[18] = {10, 13, 16, 30, 33, 23, 30, 61, 62, 45, 59, 119, 116, 90, 156, 198, 373, 326};

    int anchor_num = 3;
    int feat_w = letterbox_cols / stride;
    int feat_h = letterbox_rows / stride;
    int cls_num = 80;
    int anchor_group;
    if (stride == 8)
        anchor_group = 1;
    if (stride == 16)
        anchor_group = 2;
    if (stride == 32)
        anchor_group = 3;
    for (int h = 0; h <= feat_h - 1; h++)
    {
        for (int w = 0; w <= feat_w - 1; w++)
        {
            for (int a = 0; a <= anchor_num - 1; a++)
            {
                //process cls score
                int class_index = 0;
                float class_score = -FLT_MAX;
                for (int s = 0; s <= cls_num - 1; s++)
                {
                    float score = feat[a * feat_w * feat_h * (cls_num + 5) + h * feat_w * (cls_num + 5) + w * (cls_num + 5) + s + 5];
                    if (score > class_score)
                    {
                        class_index = s;
                        class_score = score;
                    }
                }
                //process box score
                float box_score = feat[a * feat_w * feat_h * (cls_num + 5) + (h * feat_w) * (cls_num + 5) + w * (cls_num + 5) + 4];
                float final_score = sigmoid(box_score) * sigmoid(class_score);
                if (final_score >= prob_threshold)
                {
                    int loc_idx = a * feat_h * feat_w * (cls_num + 5) + h * feat_w * (cls_num + 5) + w * (cls_num + 5);
                    float dx = sigmoid(feat[loc_idx + 0]);
                    float dy = sigmoid(feat[loc_idx + 1]);
                    float dw = sigmoid(feat[loc_idx + 2]);
                    float dh = sigmoid(feat[loc_idx + 3]);
                    float pred_cx = (dx * 2.0f - 0.5f + w) * stride;
                    float pred_cy = (dy * 2.0f - 0.5f + h) * stride;
                    float anchor_w = anchors[(anchor_group - 1) * 6 + a * 2 + 0];
                    float anchor_h = anchors[(anchor_group - 1) * 6 + a * 2 + 1];
                    float pred_w = dw * dw * 4.0f * anchor_w;
                    float pred_h = dh * dh * 4.0f * anchor_h;
                    float x0 = pred_cx - pred_w * 0.5f;
                    float y0 = pred_cy - pred_h * 0.5f;
                    float x1 = pred_cx + pred_w * 0.5f;
                    float y1 = pred_cy + pred_h * 0.5f;

                    Object obj;
                    obj.rect.x = x0;
                    obj.rect.y = y0;
                    obj.rect.width = x1 - x0;
                    obj.rect.height = y1 - y0;
                    obj.label = class_index;
                    obj.prob = final_score;
                    objects.push_back(obj);
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------//

/*-------------------------------------------
        Macros and Variables
-------------------------------------------*/
const char *usage =
    "Usage:\nnbg_name input_data1 input_data2...";
/*-------------------------------------------
                  Functions
-------------------------------------------*/
#define BILLION 1000000000
static vip_uint64_t get_perf_count()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (vip_uint64_t)((vip_uint64_t)ts.tv_nsec + (vip_uint64_t)ts.tv_sec * BILLION);
}


vip_status_e vnn_CreateInOutBufPrepareNetwork(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    int input_num = 0, i = 0;
    vip_buffer_create_params_t buf_param;

    /* Get network input num *///获取网络输入num
    status = vip_query_network(network_items->network,
                               VIP_NETWORK_PROP_INPUT_COUNT, &input_num);
    _CHECK_STATUS(status, final);
    if (input_num != network_items->input_count)
    {
        printf("Error: Graph need %d inputs, but enter %d inputs!!!\n",
               input_num, network_items->input_count);
        status = VIP_ERROR_MISSING_INPUT_OUTPUT;
        return status;
    }
    /* Create input buffers */  //创建输入缓冲区
    network_items->input_buffers = (vip_buffer *)malloc(sizeof(vip_buffer) * input_num);
    for (i = 0; i < input_num; i++)
    {
        memset(&buf_param, 0, sizeof(buf_param));
        status = vip_query_input(network_items->network, i,
                                 VIP_BUFFER_PROP_DATA_FORMAT, &buf_param.data_format);
        _CHECK_STATUS(status, final);
        status = vip_query_input(network_items->network, i,
                                 VIP_BUFFER_PROP_NUM_OF_DIMENSION, &buf_param.num_of_dims);
        _CHECK_STATUS(status, final);
        status = vip_query_input(network_items->network, i,
                                 VIP_BUFFER_PROP_SIZES_OF_DIMENSION, buf_param.sizes);
        _CHECK_STATUS(status, final);
        status = vip_query_input(network_items->network, i,
                                 VIP_BUFFER_PROP_QUANT_FORMAT, &buf_param.quant_format);
        _CHECK_STATUS(status, final);
        switch (buf_param.quant_format)
        {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            status = vip_query_input(network_items->network, i,
                                     VIP_BUFFER_PROP_FIXED_POINT_POS, &buf_param.quant_data.dfp.fixed_point_pos);
            _CHECK_STATUS(status, final);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            status = vip_query_input(network_items->network, i,
                                     VIP_BUFFER_PROP_TF_SCALE, &buf_param.quant_data.affine.scale);
            _CHECK_STATUS(status, final);
            status = vip_query_input(network_items->network, i,
                                     VIP_BUFFER_PROP_TF_ZERO_POINT, &buf_param.quant_data.affine.zeroPoint);
            _CHECK_STATUS(status, final);
            break;
        case VIP_BUFFER_QUANTIZE_NONE:
        default:
            break;
        }

        status = vip_create_buffer(&buf_param, sizeof(buf_param),
                                   &network_items->input_buffers[i]);
        _CHECK_STATUS(status, final);
    }

    /* Create output buffers */  //创建输出缓冲区
    status = vip_query_network(network_items->network,
                               VIP_NETWORK_PROP_OUTPUT_COUNT, &network_items->output_count);
    network_items->output_buffers = (vip_buffer *)malloc(sizeof(vip_buffer) * network_items->output_count);
    for (i = 0; i < network_items->output_count; i++)
    {
        memset(&buf_param, 0, sizeof(buf_param));
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_DATA_FORMAT, &buf_param.data_format);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_NUM_OF_DIMENSION, &buf_param.num_of_dims);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_SIZES_OF_DIMENSION, buf_param.sizes);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_QUANT_FORMAT, &buf_param.quant_format);
        _CHECK_STATUS(status, final);
        switch (buf_param.quant_format)
        {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            status = vip_query_output(network_items->network, i,
                                      VIP_BUFFER_PROP_FIXED_POINT_POS, &buf_param.quant_data.dfp.fixed_point_pos);
            _CHECK_STATUS(status, final);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            status = vip_query_output(network_items->network, i,
                                      VIP_BUFFER_PROP_TF_SCALE, &buf_param.quant_data.affine.scale);
            _CHECK_STATUS(status, final);
            status = vip_query_output(network_items->network, i,
                                      VIP_BUFFER_PROP_TF_ZERO_POINT, &buf_param.quant_data.affine.zeroPoint);
            _CHECK_STATUS(status, final);
            break;
        case VIP_BUFFER_QUANTIZE_NONE:
        default:
            break;
        }
        status = vip_create_buffer(&buf_param, sizeof(buf_param),
                                   &network_items->output_buffers[i]);
        _CHECK_STATUS(status, final);
    }

    /* Prepare network */ //准备网络
    status = vip_prepare_network(network_items->network);
    _CHECK_STATUS(status, final);

final:
    return status;
}

vip_status_e vnn_SetNetworkInOut(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    //char *data = NULL;
    unsigned char *data = NULL;
//    char *file_name = NULL;
//    vip_uint32_t file_size = 0;
//    vip_uint32_t buff_size = 0;
    int i;
    int con=0;
    //FILE *fp = fopen("b.txt", "a+");//打开并在指定地点创建只写文件
    /* Load input buffer data */
    for (i = 0; i < network_items->input_count; i++)
    {
        data = static_cast<unsigned char*>(vip_map_buffer(network_items->input_buffers[i]));
//        buff_size = vip_get_buffer_size(network_items->input_buffers[i]);
        // /*
       for (int i = 0; i < data1.rows; i++) {
           for (int j = 0; j < data1.cols; j++) {
               cv::Vec3b pixel = data1.at<cv::Vec3b>(i, j);
               data [con]= pixel[2];
               con++;
           }
       }
       for (int i = 0; i < data1.rows; i++) {
           for (int j = 0; j < data1.cols; j++) {
               cv::Vec3b pixel = data1.at<cv::Vec3b>(i, j);
               data [con]= pixel[1];
               con++;
           }
       }
       for (int i = 0; i < data1.rows; i++) {
           for (int j = 0; j < data1.cols; j++) {
               cv::Vec3b pixel = data1.at<cv::Vec3b>(i, j);
               data [con]= pixel[0];
               con++;
           }
       }
//        printf("---main---将图像数据放入模型--476----%d---\n",con);
        /* Set input */
        status = vip_set_input(network_items->network,
                               i, network_items->input_buffers[i]);
        _CHECK_STATUS(status, final);
    }
    /* Set output */
    for (i = 0; i < network_items->output_count; i++)
    {
        status = vip_set_output(network_items->network,
                                i, network_items->output_buffers[i]);
        _CHECK_STATUS(status, final);
    }

final:
    return status;
}
/* Initialize network items */  //初始化网络项目
vip_status_e vnn_InitNetworkItem(vip_network_items **network_items, int argc, char **argv)
{
    /*
     * argv0:   execute file
     * argv1:   nbg file
     * argv2~n: inputs n files
     */
    vip_status_e status = VIP_SUCCESS;
    vip_network_items *nnItems = NULL;
    const char *file_name = NULL;
    int input_num = 0, i = 0;
    char **inputs = NULL;
    int name_len = 0;

    file_name = (const char *)argv[1];//得到模型文件名
    input_num = argc - 1;
    if (input_num <= 0)
    {
        status = VIP_ERROR_INVALID_ARGUMENTS;
        goto final;
    }
    inputs = argv + 1;
    nnItems = (vip_network_items *)malloc(sizeof(vip_network_items));
    memset(nnItems, 0, sizeof(vip_network_items));

    name_len = strlen(file_name);
    if (name_len <= 0)
    {
        if (nnItems)
        {
            free(nnItems);
            nnItems = NULL;
        }
        status = VIP_ERROR_INVALID_ARGUMENTS;
        goto final;
    }
    nnItems->nbg_name = (char *)malloc(name_len + 1);
    memset(nnItems->nbg_name, 0, name_len + 1);
    strcpy(nnItems->nbg_name, file_name);//这行代码将变量 file_name 中的字符串复制到 nnItems->nbg_name 中。
    nnItems->input_count = input_num;
    nnItems->input_names = (char **)malloc(sizeof(char *) * input_num);
    for (i = 0; i < input_num; i++)
    {
        nnItems->input_names[i] = inputs[i];
    }

    *network_items = nnItems;

final:
    return status;
}
/* Create the neural network */  //初始化网络项创建神经网络
static vip_status_e vnn_CreateNeuralNetwork(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint64_t tmsStart, tmsEnd;
    float msVal, usVal;
    tmsStart = get_perf_count();
    status = vip_create_network(network_items->nbg_name, 0,
                                VIP_CREATE_NETWORK_FROM_FILE, &network_items->network);
    _CHECK_STATUS(status, final);
    tmsEnd = get_perf_count();
    msVal = (float)(tmsEnd - tmsStart) / 1000000;
    usVal = (float)(tmsEnd - tmsStart) / 1000;
    printf("Create Neural Network: %.2fms or %.2fus\n", msVal, usVal);
final:
    return status;
}
/* Pre process the input/output data */  //预处理输入/输出数据
static vip_status_e vnn_PreProcessNeuralNetwork(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    /* Create input/output buffers, prepare network */
    //创建输入/输出缓冲区，准备网络
    status = vnn_CreateInOutBufPrepareNetwork(network_items);
    _CHECK_STATUS(status, final);
    /* Set input/output buffers */
//    status = vnn_SetNetworkInOut(network_items);
//    _CHECK_STATUS(status, final);
final:
    return status;
}
/* Run the neural network */  //运行神经网络
vip_status_e vnn_RunNeuralNetwork(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    vip_int32_t i = 0, loop = 1;
    char *loop_s;
    vip_uint64_t tmsStart, tmsEnd, sigStart, sigEnd;
    float msVal, usVal;
    loop_s = getenv("VNN_LOOP_TIME");
    if (loop_s)
    {
        loop = atoi(loop_s);
    }
    /* Run network */
    tmsStart = get_perf_count();
//    printf("Start run graph [%d] times...\n", loop);
    for (i = 0; i < loop; i++)
    {
        sigStart = get_perf_count();
        status = vip_run_network(network_items->network);
        _CHECK_STATUS(status, final);
        sigEnd = get_perf_count();
        msVal = (float)(sigEnd - sigStart) / 1000000;
        usVal = (float)(sigEnd - sigStart) / 1000;
//        printf("Run the %d time: %.2fms or %.2fus\n", (i + 1), msVal, usVal);
    }
    tmsEnd = get_perf_count();
    msVal = (float)(tmsEnd - tmsStart) / 1000000;
    usVal = (float)(tmsEnd - tmsStart) / 1000;
//    printf("vip run network execution time:\n");
//    printf("Total   %.2fms or %.2fus\n", msVal, usVal);
//    printf("Average %.2fms or %.2fus\n", (float)msVal / loop, (float)usVal / loop);
final:
    return status;
}
static vip_float_t uint8_to_fp32(vip_uint8_t val, vip_int32_t zeroPoint, vip_float_t scale)
{
    vip_float_t result = 0.0f;

    result = (val - (vip_uint8_t)zeroPoint) * scale;
    return result;
}
vip_status_e destroy_network(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    int i = 0;

    status = vip_finish_network(network_items->network);
    _CHECK_STATUS(status, final);
    status = vip_destroy_network(network_items->network);
    _CHECK_STATUS(status, final);

    for (i = 0; i < network_items->input_count; i++)
    {
        status = vip_destroy_buffer(network_items->input_buffers[i]);
        _CHECK_STATUS(status, final);
    }
    if (network_items->input_buffers)
    {
        free(network_items->input_buffers);
        network_items->input_buffers = VIP_NULL;
    }
    for (i = 0; i < network_items->output_count; i++)
    {
        status = vip_destroy_buffer(network_items->output_buffers[i]);
        _CHECK_STATUS(status, final);
    }
    if (network_items->output_buffers)
    {
        free(network_items->output_buffers);
        network_items->output_buffers = VIP_NULL;
    }
final:
    return status;
}
void destroy_network_items(
    vip_network_items *network_items)
{
    if (network_items->nbg_name)
    {
        free(network_items->nbg_name);
        network_items->nbg_name = VIP_NULL;
    }
    if (network_items->input_names)
    {
        free(network_items->input_names);
        network_items->input_names = VIP_NULL;
    }
    if (network_items)
    {
        free(network_items);
    }
}

std::vector<float> p8_data;//定义 浮点型 变量（p8_data ）大小（1*3*52*52*85）个
std::vector<float> p16_data;
std::vector<float> p32_data;

vip_status_e save_output_data(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    int i = 0;
//#define _DUMP_FILE_LENGTH 1028
//#define _DUMP_SHAPE_LENGTH 128
//    char filename[_DUMP_FILE_LENGTH] = {0}, shape[_DUMP_SHAPE_LENGTH] = {0};
    int buff_size = 0;
    void *out_data = NULL;
    vip_buffer_create_params_t buf_param;
//    printf("###network_items->output_count###%d###\n",network_items->output_count);//3
    for (i = 0; i < network_items->output_count; i++)
    {
        buff_size = vip_get_buffer_size(network_items->output_buffers[i]);
        if (buff_size <= 0)
        {
            status = VIP_ERROR_IO;
            return status;
        }
        memset(&buf_param, 0, sizeof(buf_param));
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_DATA_FORMAT, &buf_param.data_format);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_NUM_OF_DIMENSION, &buf_param.num_of_dims);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_SIZES_OF_DIMENSION, buf_param.sizes);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_TF_ZERO_POINT, &buf_param.quant_data.affine.zeroPoint);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i,
                                  VIP_BUFFER_PROP_TF_SCALE, &buf_param.quant_data.affine.scale);
        _CHECK_STATUS(status, final);
        out_data = vip_map_buffer(network_items->output_buffers[i]);

        vip_int32_t zeroPoint = buf_param.quant_data.affine.zeroPoint;
        vip_float_t scale = buf_param.quant_data.affine.scale;
        vip_uint8_t *data = (vip_uint8_t *)out_data;
        float *fp_data = (float *)malloc(buff_size * sizeof(float));
//        printf("data_format=%d buff_size=%d\n", buf_param.data_format, buff_size); // VIP_BUFFER_FORMAT_UINT8=2
        for (int j = 0; j < buff_size; j++)
        {
            fp_data[j] = uint8_to_fp32(*data, zeroPoint, scale);
            if (i==0)
            {
                /*
                vector<int> v1;  //空vector对象
                for (int i = 0; i != 100; ++i)
                    v1.push_back(i);   //依次把整数值放到v1的尾端
                for (int i = 0; i != 100; ++i)
                    //v1.push_back(i);   //依次把整数值放到v1的尾端
                    printf("v1.push_back(i)----#%d----111-----\n",v1[i]);//a.insert(a.end(),10,5) 在末尾插入10个值为5的元素
                //循环结束后v1有100个元素，值从0到99
                */
                p8_data.push_back(fp_data[j]);
            }
            if (i==1)
            {
                p16_data.push_back(fp_data[j]);                      //a.insert(a.end(),10,5) 在末尾插入10个值为5的元素
            }
            if (i==2)
            {
                 p32_data.push_back(fp_data[j]);                      //a.insert(a.end(),10,5) 在末尾插入10个值为5的元素
            }
            data += 1; // VIP_BUFFER_FORMAT_UINT8 one byte
        }
        free(fp_data);
    }

final:
    return status;
}
static int detect_yolov5(const cv::Mat& bgr, std::vector<Object>& objects)
{
    std::chrono::steady_clock::time_point Tbegin, Tend;
    Tbegin = std::chrono::steady_clock::now();
    // set default letterbox size
    int letterbox_rows = hw;
    int letterbox_cols = hw;
     /* postprocess */
    const float prob_threshold = 0.5f;
    const float nms_threshold = 0.45f;
    std::vector<Object> proposals;
    std::vector<Object> objects8;
    std::vector<Object> objects16;
    std::vector<Object> objects32;
//    for (int i = 0; i<8; i++)
//        printf("-------#%f---------\n",p8_data.data()[i]);
    /*generate_proposals(32, p32_data.data(), prob_threshold, objects32, letterbox_cols, letterbox_rows) 函数的作用是生成对象建议或感兴趣区域（regions of interest）。
    具体解释如下：
    32 是生成建议的数量。这个参数指定要生成多少个潜在的感兴趣区域，也就是对象建议。
    p32_data.data() 提供了 generate_proposals() 函数所需的数据。这个部分假设 p32_data 是一个 std::vector<float>，通过调用 .data() 来获取向量底层数据的指针。
    prob_threshold 是一个阈值，用于基于概率值对对象建议进行过滤。只有概率高于阈值的建议才会被考虑。
    objects32 是一个 std::vector<Object>，用于存储生成的对象建议。函数将生成的建议填充到这个向量中。
    letterbox_cols 和 letterbox_rows 是与正在处理的图像或数据结构的大小或维度相关的参数。这些值很可能指定执行建议生成所需的尺寸或调整建议以适应特定情况。
    总体而言，generate_proposals() 函数的调用目的是从提供的数据（p32_data）中生成一定数量的对象建议，同时遵守概率阈值。生成的建议将存储在 objects32 中。*/
    generate_proposals(32, p32_data.data(), prob_threshold, objects32, letterbox_cols, letterbox_rows);
    proposals.insert(proposals.end(), objects32.begin(), objects32.end());
    generate_proposals(16, p16_data.data(), prob_threshold, objects16, letterbox_cols, letterbox_rows);
    proposals.insert(proposals.end(), objects16.begin(), objects16.end());
    generate_proposals(8, p8_data.data(), prob_threshold, objects8, letterbox_cols, letterbox_rows);
    proposals.insert(proposals.end(), objects8.begin(), objects8.end());
    qsort_descent_inplace(proposals);
    std::vector<int> picked;
    nms_sorted_bboxes(proposals, picked, nms_threshold);
    /* yolov5 draw the result *///绘制结果
    float scale_letterbox;
    int resize_rows;
    int resize_cols;
    if ((letterbox_rows * 1.0 / bgr.rows) < (letterbox_cols * 1.0 / bgr.cols))
    {
        scale_letterbox = letterbox_rows * 1.0 / bgr.rows;
    }
    else
    {
        scale_letterbox = letterbox_cols * 1.0 / bgr.cols;
    }
    resize_cols = int(scale_letterbox * bgr.cols);
    resize_rows = int(scale_letterbox * bgr.rows);
    int tmp_h = (letterbox_rows - resize_rows) / 2;
    int tmp_w = (letterbox_cols - resize_cols) / 2;
    float ratio_x = (float)bgr.rows / resize_rows;
    float ratio_y = (float)bgr.cols / resize_cols;
    int count = picked.size();
    fprintf(stderr, "detection num: %d\n", count);

    objects.resize(count);
    for (int i = 0; i < count; i++)
    {
        objects[i] = proposals[picked[i]];
        float x0 = (objects[i].rect.x);
        float y0 = (objects[i].rect.y);
        float x1 = (objects[i].rect.x + objects[i].rect.width);
        float y1 = (objects[i].rect.y + objects[i].rect.height);
        x0 = (x0 - tmp_w) * ratio_x;
        y0 = (y0 - tmp_h) * ratio_y;
        x1 = (x1 - tmp_w) * ratio_x;
        y1 = (y1 - tmp_h) * ratio_y;
        x0 = std::max(std::min(x0, (float)(bgr.cols - 1)), 0.f);
        y0 = std::max(std::min(y0, (float)(bgr.rows - 1)), 0.f);
        x1 = std::max(std::min(x1, (float)(bgr.cols - 1)), 0.f);
        y1 = std::max(std::min(y1, (float)(bgr.rows - 1)), 0.f);
        objects[i].rect.x = x0;
        objects[i].rect.y = y0;
        objects[i].rect.width = x1 - x0;
        objects[i].rect.height = y1 - y0;
    }
    Tend = std::chrono::steady_clock::now();
    float f = std::chrono::duration_cast <std::chrono::milliseconds> (Tend - Tbegin).count();
    std::cout << "time : " << f/1000.0 << " Sec" << std::endl;
    return 0;
}
static void draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects)
{
    static const char* class_names[] = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
        "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
        "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
        "hair drier", "toothbrush"};

    cv::Mat image = bgr.clone();
    for (size_t i = 0; i < objects.size(); i++)
    {
        const Object& obj = objects[i];
        fprintf(stderr, "%2d: %3.0f%%, [%4.0f, %4.0f, %4.0f, %4.0f], %s\n", obj.label, obj.prob * 100, obj.rect.x,
                obj.rect.y, obj.rect.x + obj.rect.width, obj.rect.y + obj.rect.height, class_names[obj.label]);
        cv::rectangle(image, obj.rect, cv::Scalar(255, 0, 0));
        char text[256];
        sprintf(text, "%s %.1f%%", class_names[obj.label], obj.prob * 100);
        int baseLine = 0;
        cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
        int x = obj.rect.x;
        int y = obj.rect.y - label_size.height - baseLine;
        if (y < 0)
            y = 0;
        if (x + label_size.width > image.cols)
            x = image.cols - label_size.width;
        cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                      cv::Scalar(255, 255, 255), -1);
        cv::putText(image, text, cv::Point(x, y + label_size.height), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(0, 0, 0));
    }
    cv::Mat rgb_image;
    cv::cvtColor(image, rgb_image, cv::COLOR_BGR2RGB);
//    cv::imwrite("yolov5_out.jpg", image);


//    cv::Mat data1=cv::imread("416_416bus.jpg");
    const int frame_width = 640;
//yolov5_out.jpg
    std::ofstream ofs("/dev/fb0");
//    cv::Mat frame=cv::imread("416_416bus.jpg");;
//    cv::Mat frame=image;
    cv::Size2f frame_size = image.size();
    cv::Mat framebuffer_compat;
    std::vector<cv::Mat> split_bgr;
    cv::split(image, split_bgr);
    split_bgr.push_back(cv::Mat(frame_size, CV_8UC1, cv::Scalar(255)));
    cv::merge(split_bgr, framebuffer_compat);
    for (int y = 0; y < frame_size.height; y++) {
        ofs.seekp(y * frame_width * 3);
        ofs.write(reinterpret_cast<char*>(framebuffer_compat.ptr(y)), frame_size.width * 3);
    }



}
/* Post process output data */  //后处理输出数据
vip_status_e vnn_PostProcessNeuralNetwork(vip_network_items *network_items)
{
    return save_output_data(network_items);
}
vip_status_e vnn_ReleaseNeuralNetwork(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    status = destroy_network(network_items);
    _CHECK_STATUS(status, final);
    destroy_network_items(network_items);
    status = vip_destroy();
    _CHECK_STATUS(status, final);
final:
    return status;
}
/*-------------------读取摄像头程序---------------------------------*/
#include <linux/videodev2.h>
#define FILE_VIDEO  "/dev/video0"

//#define WIDTH  1920
//#define HEIGHT 1088
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
    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if(-1 == ioctl(fd,VIDIOC_REQBUFS,&reqbufs)){
        perror("Fail to ioctl 'VIDIOC_REQBUFS'");
        exit(EXIT_FAILURE);
    }
    n_buffer = reqbufs.count;
//    printf("n_buffer = %d\n", n_buffer);
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
//        printf("usr_buf[%d].start=0x%x\n",n_buffer,usr_buf[n_buffer].start);
        if(MAP_FAILED == usr_buf[n_buffer].start)
        {
            perror("Fail to mmap");
            exit(EXIT_FAILURE);
        }
//        printf("usr_buf %d: address=0x%x, length=%d\n", n_buffer, (unsigned int)usr_buf[n_buffer].start, usr_buf[n_buffer].length);
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
    printf("VIDIOC_S_INPUT finish!!\n");
    //VIDIOC_ENUM_FMT用于枚举视频设备支持的视频格式
    while(ioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0)
    {
        fmt.index++;
        //printf("pixelformat = ''%c%c%c%c''\ndescription = ''%s''\n",fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,(fmt.pixelformat >> 16) & 0xFF,                   (fmt.pixelformat    >> 24) & 0xFF,fmt.description);
    }
    //check video decive driver capability  ，//用于查询视频设备的能力
    ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if(ret < 0){
        perror("Fail to ioctl VIDEO_QUERYCAP");
        exit(EXIT_FAILURE);
    }
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
    char fmtstr[8];
    memset(fmtstr, 0, 8);
    memcpy(fmtstr, &tv_fmt.fmt.pix.pixelformat, 4);
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
//        printf("i=%d.\n", i);
        perror("VIDIOC_STREAMON");
        close(fd);
        exit(EXIT_FAILURE);
    }
    return 0;
}
int process_image_video(void *addr, int length)
{
    int r;
    cv::Mat yuv(1088* 1.5, 1920, CV_8UC1, addr);
    cv::Mat bgr;
    cv::Mat roi;
    cv::cvtColor(yuv, bgr, CV_YUV2BGR_NV21);
//    roi方法1
//    int x = (bgr.cols - hw) / 2;
//    int y = (bgr.rows - hw) / 2;
    //cv::resize(bgr, data1, cv::Size(640, 0));
    cv::resize(bgr, roi,cv::Size(640, 320), 0, 0, cv::INTER_AREA);//先将图片缩放到1129，640（1088/640=1.7，1920/1.7=1129.411），所以选择1129，640
//    cv::resize(bgr, roi,cv::Size(1129, 640), 0, 0, cv::INTER_AREA);//先将图片缩放到1129，640（1088/640=1.7，1920/1.7=1129.411），所以选择1129，640
    int x = (roi.cols - hw) / 2;
    int y = (roi.rows - hw) / 2;
    data1= roi(cv::Range(y, y + hw), cv::Range(x, x + hw)).clone();//裁减成640*640的图像来推理
    //cv::imwrite("/home/v4l2_yuv_video.jpg", data1);
    //echo put   /root/yolov5_out.jpg  yolov5_out.jpg  | sshpass -p '8888' sftp -oPort=22 -o StrictHostKeyChecking=yes fawen@192.168.134.148
    //usleep(500);
    // 执行SFTP命令
     //r = system("echo put  /home/v4l2_yuv_video.jpg  v4l2_yuv_video.jpg  | sshpass -p '8888' sftp -oPort=22 -o StrictHostKeyChecking=no fawen@192.168.190.148");
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
//    printf("buf.index=%d,n_buffer=%d\n",buf.index,n_buffer);
//    printf(" usr_buf[%d].start=0x%x,usr_buf[%d].length=0x%x\n", buf.index,usr_buf[buf.index].start,buf.index,usr_buf[buf.index].length);
    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf.type)
        bytesused = buf.m.planes[0].bytesused;
    else
        bytesused = buf.bytesused;
//     printf("read_frame bytesused=%d\n",bytesused);
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
    int count =1;
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
            /*这行代码的作用是在给定的时间内等待文件描述符（fd）上是否有可读事件发生。如果有可读事件，则该函数会返回一个正值，否则会返回0或-1*/
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
int main(int argc, char** argv)
{
    int ret;
    int fd;
    fd = open_camera_device();
    printf("100ask v853 open_camera_device\n");
    printf("fd = %d\n",fd);
    init_camera_device(fd);
    ret = isp_init(0);
    ret = isp_run(0);
    start_capture(fd);
    printf("100ask v853start_capture\n");
    mainloop(fd);
    printf("100ask v853 mainloop\n");
//    stop_capture(fd);
    printf("100ask v853 stop_capture\n");
//    close_camera_device(fd);
    ret = isp_stop(0);
//    ret = isp_pthread_join(0);
    ret = isp_exit(0);
    printf("-------close_camera_device-------\n");
    int r;

    vip_status_e status = VIP_SUCCESS;
    vip_network_items *network_items = VIP_NULL;
    printf("argc %d\n", argc);
    printf("argv[0] %s\n", argv[0]);
    printf("argv[1] %s\n", argv[1]);
    printf("argv[2] %s\n", argv[2]);
    std::vector<Object> objects;
    //return 0;
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [model path]\n", argv[0]);
        return -1;
    }
    printf("输入参数个数 %d!\n", argc);
    printf("输入参数argv %s--%s--%s\n", argv[0],argv[1],argv[2]);
    if (argc < 2)
    {
        printf("%s\n", usage);
        printf("Arguments count %d is incorrect!\n", argc);
        return -1;
    }
    /* Initialize  vip lite */
    status = vip_init(9 * 1024 * 1024);
    _CHECK_STATUS(status, final);
    printf("Initialize  vip lite---ok---!\n");
    /* Initialize network items */  //初始化网络项目
    status = vnn_InitNetworkItem(&network_items, argc, argv);
    _CHECK_STATUS(status, final);
    printf("Initialize network items---ok---!\n");

    /* Create the neural network */  //初始化网络项创建神经网络
    status = vnn_CreateNeuralNetwork(network_items);
    _CHECK_STATUS(status, final);
    printf("Create the neural network ---ok---!\n");
    status = vnn_PreProcessNeuralNetwork(network_items);
    _CHECK_STATUS(status, final);
    ///*
    for (int i=0; i<1000000;i++)
    {
        mainloop(fd);
        //Pre process the input/output data   //预处理输入/输出数据 将模型数据送到网络
        status = vnn_SetNetworkInOut(network_items);//将摄像头数据送到网络
        _CHECK_STATUS(status, final);
//        printf("Pre process the input/output data ---ok---!\n");
        // Run the neural network   //运行神经网络
         status = vnn_RunNeuralNetwork(network_items);
         _CHECK_STATUS(status, final);
//        printf("Run the neural network ---ok---!\n");
        //Post process output data  //后处理输出数据
        status = vnn_PostProcessNeuralNetwork(network_items);
        _CHECK_STATUS(status, final);
        detect_yolov5(data1, objects);
        draw_objects(data1, objects);
        p8_data.clear();
        p16_data.clear();
        p32_data.clear();
        data1.setTo(0);
//        mainloop(fd);
//        fd = open_camera_device();
//        init_camera_device(fd);
//        start_capture(fd);
//        mainloop(fd);
//        stop_capture(fd);
//        close_camera_device(fd);
        //r = system("sync && echo 3 > /proc/sys/vm/drop_caches");//释放内存

    }
    //*/
    /*// Pre process the input/output data ///预处理输入/输出数据 将模型数据送到网络
    status = vnn_PreProcessNeuralNetwork(network_items);
    _CHECK_STATUS(status, final);
    status = vnn_SetNetworkInOut(network_items);//将摄像头数据送到网络
    _CHECK_STATUS(status, final);
    printf("Pre process the input/output data ---ok---!\n");

     Run the neural network   //运行神经网络
     status = vnn_RunNeuralNetwork(network_items);
     _CHECK_STATUS(status, final);
    printf("Run the neural network ---ok---!\n");

     Post process output data   //后处理输出数据
    status = vnn_PostProcessNeuralNetwork(network_items);
    _CHECK_STATUS(status, final);
//----------------------------------------------------------------------------------------------//
    detect_yolov5(data1, objects);
    draw_objects(data1, objects);
    */
    //system("ls -al /etc/passwd /etc/shadow");
    //system("reboot");
    //r=system("sshpass -p '8888' scp  -P 38940 /root/yolov5_out.jpg  fawen@73l984h497.zicp.fun:/yolov5_out.jpg");
    //-oPort=22 -o StrictHostKeyChecking=yes
    //echo put   /root/yolov5_out.jpg  yolov5_out.jpg  | sshpass -p '8888' sftp -oPort=22 -o StrictHostKeyChecking=yes fawen@192.168.134.148
    // 执行SFTP命令
    //r = system(sftp_command.c_str());
    //r=system("sshpass -p '8888' scp -P 22 -o StrictHostKeyChecking=yes /root/yolov5_out.jpg fawen@192.168.134.148:/yolov5_out.jpg");//
    printf("传输状态%d--\n",r);
    stop_capture(fd);
    printf("-------stop_capture-111------\n");
    close_camera_device(fd);
    //ret = isp_stop(0);
    //ret = isp_pthread_join(0);
    //ret = isp_exit(0);
    r = system("sync && echo 3 > /proc/sys/vm/drop_caches");//释放内存
final:
    /* Destroy resources *///终止 VIPLite 驱动程序，释放 vip_init（）请求的资源，并关闭 vip hard
    status = vnn_ReleaseNeuralNetwork(network_items);
    printf("Destroy resources ---ok---!\n");
    //stop_capture(fd);
    printf("-------stop_capture----222---\n");
    close_camera_device(fd);
    //ret = isp_stop(0);
    //ret = isp_pthread_join(0);
    //ret = isp_exit(0);
    r = system("sync && echo 3 > /proc/sys/vm/drop_caches");//释放内存
    return status;
}

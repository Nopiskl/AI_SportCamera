# test_account/CMakeLists.txt

# 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(yolov3)

set( CMAKE_C_COMPILER arm-openwrt-linux-gcc)
set( CMAKE_CXX_COMPILER arm-openwrt-linux-g++)
# 添加执行文件
#add_executable(yolov3 main.cpp)
#set(yolov3 main.cpp )
add_executable(yolov3 main.cpp
     bmp.h
      box.c
      box.h
     vnn_global.h
      vnn_post_process.c
      vnn_post_process.h
      vnn_pre_process.c
      vnn_pre_process.c
      vnn_pre_process.h
      yolo_layer.c
      yolo_layer.h
      yolo_v3_post_process.c
        )
# 添加头文件目录，如果不添加，找不到头文件
target_include_directories(yolov3 PUBLIC "include")
# 添加库文件目录，如果不添加，找不到库文件
target_link_directories(yolov3 PUBLIC "lib")
# 添加目标链接库
target_link_libraries(yolov3 PRIVATE VIPlite VIPuser)  
#target_link_libraries(yolov3 PRIVATE VIPuser) 




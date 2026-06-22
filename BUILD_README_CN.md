# V851S BSP 构建说明

语言： [English](BUILD_README.md) | **中文**

本文档用于记录 V851S BSP 的具体构建流程。项目首页仅保留路线选择与模块说明，详细编译步骤统一维护在此处。

## 构建路线

根据开发目标，建议在以下两种方式中选择：

| 方式 | 适用场景 | MPP 支持 | 推荐程度 |
| --- | --- | --- | --- |
| 全志开源社区 SDK | 需要从相对纯净的 BSP 环境开始验证 | 不包含完整 MPP 支持 | 一般 |
| Docker 环境 | 需要开发图像、视频、AI 推理与 MPP 加速能力 | 已预配置 MPP 支持 | 推荐 |

如果目标是开发 Application、MPP Pipeline、YOLO 实时检测或视频编解码相关功能，建议直接使用 Docker 方式。

## 板级包选择

根据要构建的硬件版本选择一套 BSP 覆盖包：

| 版本 | 覆盖包目录 | 推荐用途 |
| --- | --- | --- |
| DVP | `TinaSDKv5.0/Project_for_DVP` | BSP 基础验证、DVP 摄像头调试和基于 OpenCV 的应用开发。MPP 链路未完整测试。 |
| MIPI | `TinaSDKv5.0/Project_for_MIPI` | 完整 Application 开发、LCD 硬件加速、AI_ISP、OpenCV 与 MPP 链路验证。 |

两套覆盖包的板级名称均为 `nopiskl`，lunch 目标为 `v851s-nopiskl-tina`。

MPP 与 E907 开发包通常可在 100ASK / 柚木 PI-V851S 资料或带 MPP 支持的 Docker 环境中获取。MPP 使用参考：<https://forums.100ask.net/t/topic/3107>，E907 开发参考：<https://forums.100ask.net/t/topic/7119>。

## 方式一：基于全志开源社区 SDK 构建

> 该方式更适合 BSP 基础验证，不推荐用于 MPP 相关功能开发。

1. 按照全志开源社区文档获取 SDK：

   [全志开源社区 SDK 获取指南](https://v853.docs.aw-ol.com/study/study_3getsdktoc/)

2. 下载本仓库中的 BSP 补充文件，选择一套覆盖包，并覆盖到 SDK 对应目录。

   补充内容主要包括：

   - 驱动文件
   - 补丁文件
   - TinaTarget 文件

   DVP 版本：

   ```bash
   cp -rf TinaSDKv5.0/Project_for_DVP/* ${SDK_PATH}/
   ```

   MIPI 版本：

   ```bash
   cp -rf TinaSDKv5.0/Project_for_MIPI/* ${SDK_PATH}/
   ```

3. 应用内核补丁：

   ```bash
   cd ${SDK_PATH}/kernel/linux-4.9
   git apply ${SDK_PATH}/patch/kernel/*.patch
   ```

4. 进入 SDK 根目录，执行编译命令：

   ```bash
   source build/envsetup.sh
   lunch v851s-nopiskl-tina
   make
   ```

5. 编译完成后执行固件打包：

   ```bash
   pack
   ```

6. 固件默认输出路径：

   ```text
   out/v851s/nopiskl/openwrt/v851s_linux_nopiskl_uart0.img
   ```

## 方式二：基于 Docker 构建（推荐）

> 推荐用于图像处理、视频链路、AI 推理和 MPP 加速相关开发。

1. 参考柚木 PI-V851S 提供的资料与 Docker 环境：

   [柚木 PI-V851S 资料汇总 - Allwinner / 柚木 PI-V851S 开发社区](https://forums.100ask.net/t/topic/3009)

   该 Docker 环境已预配置全志 MPP 支持，可减少环境搭建成本。

2. 下载本仓库中的 BSP 补充文件，选择一套覆盖包，并覆盖到 SDK 对应目录。

   补充内容主要包括：

   - 驱动文件
   - 补丁文件
   - TinaTarget 文件

   DVP 版本：

   ```bash
   cp -rf TinaSDKv5.0/Project_for_DVP/* ${SDK_PATH}/
   ```

   MIPI 版本：

   ```bash
   cp -rf TinaSDKv5.0/Project_for_MIPI/* ${SDK_PATH}/
   ```

3. 应用内核补丁：

   ```bash
   cd ${SDK_PATH}/kernel/linux-4.9
   git apply ${SDK_PATH}/patch/kernel/*.patch
   ```

4. 进入 SDK 根目录，执行编译命令：

   ```bash
   source build/envsetup.sh
   lunch v851s-nopiskl-tina
   make
   ```

5. 编译完成后执行固件打包：

   ```bash
   pack
   ```

6. 固件默认输出路径：

   ```text
   out/v851s/nopiskl/openwrt/v851s_linux_nopiskl_uart0.img
   ```

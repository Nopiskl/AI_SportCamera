# V851S BSP Build Instructions

Language: **English** | [中文](BUILD_README_CN.md)

This document records the detailed build process for the V851S BSP. The project homepage keeps only route selection and module descriptions, while the detailed compilation steps are maintained here.

## Build Routes

Choose one of the following routes according to your development target:

| Route | Suitable Scenarios | MPP Support | Recommendation |
| --- | --- | --- | --- |
| Allwinner open-source community SDK | BSP validation from a relatively clean environment. | Does not include full MPP support. | General |
| Docker environment | Image processing, video pipeline, AI inference, and MPP acceleration development. | MPP support is preconfigured. | Recommended |

If your target is the Application, MPP pipeline, real-time YOLO detection, or video encoding/decoding features, the Docker route is recommended.

## Board Package Selection

Choose one BSP overlay according to the hardware version you want to build:

| Version | Overlay Directory | Recommended Usage |
| --- | --- | --- |
| DVP | `TinaSDKv5.0/Project_for_DVP` | Basic BSP validation, DVP camera bring-up, and OpenCV-based application development. MPP has not been fully tested on this route. |
| MIPI | `TinaSDKv5.0/Project_for_MIPI` | Full Application development, LCD hardware acceleration, AI_ISP, OpenCV, and MPP pipeline verification. |

Both overlays use the board name `nopiskl`; the lunch target is `v851s-nopiskl-tina`.

The MPP and E907 development packages are usually obtained together with the 100ASK / Youmu PI-V851S resources or the MPP-enabled Docker environment. See the MPP usage reference at <https://forums.100ask.net/t/topic/3107> and the E907 development reference at <https://forums.100ask.net/t/topic/7119>.

## Route 1: Build with the Allwinner Open-Source Community SDK

> This route is more suitable for basic BSP validation and is not recommended for MPP-related feature development.

1. Obtain the SDK by following the Allwinner open-source community documentation:

   [Allwinner open-source community SDK acquisition guide](https://v853.docs.aw-ol.com/study/study_3getsdktoc/)

2. Download the BSP supplemental files from this repository, choose one overlay, and copy it over the corresponding SDK directories.

   The supplemental content mainly includes:

   - Driver files
   - Patch files
   - TinaTarget files

   DVP version:

   ```bash
   cp -rf TinaSDKv5.0/Project_for_DVP/* ${SDK_PATH}/
   ```

   MIPI version:

   ```bash
   cp -rf TinaSDKv5.0/Project_for_MIPI/* ${SDK_PATH}/
   ```

3. Apply the kernel patches:

   ```bash
   cd ${SDK_PATH}/kernel/linux-4.9
   git apply ${SDK_PATH}/patch/kernel/*.patch
   ```

4. Enter the SDK root directory and run the build commands:

   ```bash
   source build/envsetup.sh
   lunch v851s-nopiskl-tina
   make
   ```

5. Package the firmware after compilation finishes:

   ```bash
   pack
   ```

6. The default firmware output path is:

   ```text
   out/v851s/nopiskl/openwrt/v851s_linux_nopiskl_uart0.img
   ```

## Route 2: Build with Docker (Recommended)

> Recommended for image processing, video pipeline, AI inference, and MPP acceleration development.

1. Refer to the resources and Docker environment provided for Youmu PI-V851S:

   [Youmu PI-V851S resource collection - Allwinner / Youmu PI-V851S development community](https://forums.100ask.net/t/topic/3009)

   This Docker environment has preconfigured Allwinner MPP support and can reduce environment setup cost.

2. Download the BSP supplemental files from this repository, choose one overlay, and copy it over the corresponding SDK directories.

   The supplemental content mainly includes:

   - Driver files
   - Patch files
   - TinaTarget files

   DVP version:

   ```bash
   cp -rf TinaSDKv5.0/Project_for_DVP/* ${SDK_PATH}/
   ```

   MIPI version:

   ```bash
   cp -rf TinaSDKv5.0/Project_for_MIPI/* ${SDK_PATH}/
   ```

3. Apply the kernel patches:

   ```bash
   cd ${SDK_PATH}/kernel/linux-4.9
   git apply ${SDK_PATH}/patch/kernel/*.patch
   ```

4. Enter the SDK root directory and run the build commands:

   ```bash
   source build/envsetup.sh
   lunch v851s-nopiskl-tina
   make
   ```

5. Package the firmware after compilation finishes:

   ```bash
   pack
   ```

6. The default firmware output path is:

   ```text
   out/v851s/nopiskl/openwrt/v851s_linux_nopiskl_uart0.img
   ```

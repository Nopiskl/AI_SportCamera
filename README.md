# V851S BSP Build and Development Guide

Language: **English** | [中文](README_CN.md)

This repository contains a BSP project for the **Allwinner V851S** platform, including board-level configuration, driver patches, OpenWrt/TinaTarget adaptation, AI camera application examples, and MPP / OpenCV / V4L2 development references.

The program can run on Lizard-compatible boards. If you have the development board, you can also test this project directly on the hardware.

> The main Application program is still under active iteration and is not open source at the moment. This repository keeps the BSP, board-side examples, MPP demos, and external modules to support firmware builds, feature verification, and secondary development.

| Qt UI Preview | DVP Hardware Project |
| --- | --- |
| <img src="Application/1.png" alt="Qt UI preview" width="100%"> | <img src="Hardware/Project_for_DVP/2.png" alt="DVP hardware project" width="100%"> |

<p align="center">
  <img src="Hardware/Project_for_MIPI/1.png" alt="DVP PCB render preview" width="520">
</p>

## Board-Level Configurations

This repository keeps two board-level hardware and BSP package sets. Both BSP packages expose the board target as **nopiskl**.

| Version | Hardware Files | BSP Overlay | Advantages | Notes |
| --- | --- | --- | --- | --- |
| DVP | `Hardware/Project_for_DVP` | `TinaSDKv5.0/Project_for_DVP` | Uses a mainline-style LCD driver and a DVP camera. ISP setup is not required, so bring-up and debugging are simpler. | MPP has not been fully tested on this route. It is recommended to use the OpenCV path of the main program first. |
| MIPI | `Hardware/Project_for_MIPI` | `TinaSDKv5.0/Project_for_MIPI` | Supports the full main Application feature set, LCD hardware acceleration, and AI_ISP. It is compatible with both the OpenCV path and the MPP path. | The development cycle is longer and it depends more heavily on the vendor SDK stack. |

## Project Features

The Application uses **Qt** as the UI management framework and is designed for AI sport camera scenarios. It covers core capabilities such as photo capture, video recording, real-time YOLO detection, and RTSP streaming.

| Capability | Description |
| --- | --- |
| Graphical UI | Builds the main interface with Qt and manages photo capture, video recording, preview, and interaction flows. |
| AI Detection | Supports real-time YOLO detection, including both an MPP hardware-accelerated path and a native OpenCV capture path. |
| Video Processing | Integrates the Allwinner MPP pipeline to improve frame processing, encoding, and data transfer efficiency. |
| Network Streaming | Supports RTSP streaming for remote preview and video pipeline debugging. |
| Development Examples | Provides OpenCV / V4L2 / ISP / MPP examples for easier understanding and migration. |

## Main Application

`Application` is the main process project for the AI sport camera. It mainly integrates the MPP processing path, the generic OpenCV / V4L2 path, and Qt UI logic.

The Application is designed to balance ease of use and performance:

| Path | Advantages | Suitable Scenarios |
| --- | --- | --- |
| OpenCV / V4L2 | Clear structure, low migration cost, and fewer dependencies on the SoC ecosystem. | Application logic verification and fast secondary development. |
| MPP Pipeline | Makes deeper use of the Allwinner SoC ecosystem and hardware acceleration for higher performance. | Real-time video processing, encoding and streaming, and the main AI camera pipeline. |

The Application should be developed together with the MPP modules. Although the component libraries have been split out separately and no longer strictly require Docker, a Docker environment with MPP support is still recommended for full MPP feature verification.

## General Board-Side Examples

The MPP path is feature-complete but has a higher learning curve, so this repository also provides two board-side examples that are easier to migrate. They help demonstrate camera capture, AI inference, and OpenCV processing flows.

| Module | Path | Description |
| --- | --- | --- |
| YOLOv8 OpenCV example | `TinaSDKv5.0/Project_for_DVP/openwrt/package/nopiskl/yolov8` or `TinaSDKv5.0/Project_for_MIPI/openwrt/package/nopiskl/yolov8` | Implements YOLOv8 detection and classification with OpenCV, without MPP. The frame rate is relatively low. It is compiled into the filesystem by default, can be run directly from the board terminal, and is also integrated into the main Application. |
| YOLOv5 GC2053 example | `TinaSDKv5.0/Project_for_MIPI/nopiskl_external/yolov5_opencv_gc2053` | Uses the GC2053 camera and implements YOLOv5 detection through standard V4L2 + ISP APIs. This example is closer to the lower-level stack and requires some embedded development experience. |

## MPP Modules

MPP-related examples are located at:

```text
Application/mpp
```

The MPP and E907 development packages are usually obtained together with the Nopiskl's AI_SecurityRecorder release. Usage references:

- [MPP usage reference](https://forums.100ask.net/t/topic/3107)
- [E907 development reference](https://forums.100ask.net/t/topic/7119)

If you want to study the integrated Application features, or improve frame rate and data transfer efficiency through the MPP pipeline, use a Docker environment with MPP support.

The software under `Application/mpp` should be copied into the MPP demo directory before use. Most of the main Application's video path, encoding path, and streaming features are strongly related to MPP.

## Build Routes

Choose one of the following routes according to your development target:

| Route | Suitable Scenarios | MPP Support | Recommendation |
| --- | --- | --- | --- |
| Allwinner open-source community SDK | BSP validation from a relatively clean environment. | Does not include full MPP support. | General |
| Docker environment | Image processing, video pipeline, AI inference, and MPP acceleration development. | MPP support is preconfigured. | Recommended |

If your target is the Application, MPP pipeline, real-time YOLO detection, or video encoding/decoding features, the Docker route is recommended.

For detailed build steps, see [Build Instructions](BUILD_README.md).

## Key Configuration Files

| Type | Path |
| --- | --- |
| DVP board overlay | `TinaSDKv5.0/Project_for_DVP/` |
| MIPI board overlay | `TinaSDKv5.0/Project_for_MIPI/` |
| Device tree and board-level configuration | `device/config/chips/v851s/configs/nopiskl/board.dts` |
| U-Boot boot environment | `device/config/chips/v851s/configs/nopiskl/env.cfg` |
| U-Boot defconfig | `brandy/brandy-2.0/u-boot-2018/configs/sun8iw21p1_defconfig` |
| Board-side application projects | `openwrt/package/nopiskl/` |

> The paths without the `TinaSDKv5.0/Project_for_*` prefix are the final locations after copying one overlay into a Tina SDK root directory.

## Allwinner Framework References

The Allwinner platform uses its own **disp framework** for display and image processing instead of DRM, and uses the **vin framework** to manage V4L2 subdevs.

The following resources are recommended for understanding the V851S media pipeline, driver structure, and Tina SDK workflow:

- [Tina Linux development resources](https://tina.100ask.net/)
- [Allwinner open-source community SDK acquisition guide](https://v853.docs.aw-ol.com/study/study_3getsdktoc/)
- [Youmu PI-V851S development resources](https://forums.100ask.net/t/topic/3009)

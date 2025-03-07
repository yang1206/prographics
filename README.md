# ProGraphics

ProGraphics 是一个基于 Qt 和 OpenGL 的现代化数据可视化图表库，提供了简单的 3D 图表渲染功能。

[![Windows Build](https://github.com/yang1206/prographics/actions/workflows/windows-build.yml/badge.svg)](https://github.com/yang1206/prographics/actions/workflows/windows-build.yml)
[![macOS Build](https://github.com/yang1206/prographics/actions/workflows/macos-build.yml/badge.svg)](https://github.com/yang1206/prographics/actions/workflows/macos-build.yml)
[![License: LGPL v3](https://img.shields.io/badge/License-LGPL%20v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)


## 特性

- 基于现代 OpenGL (4.1 Core Profile) 的高性能渲染
- 专业的 3D 数据可视化图表组件
- 灵活的相机控制系统
- 高质量文本渲染
- 支持材质和纹理系统
- 跨平台支持 (Windows & macOS)

## 系统要求

### 编译器支持
- Windows: MSVC 2019 或更高版本
- macOS: AppleClang 11.0 或更高版本

### 依赖项
- CMake 3.16+
- Qt 6.0+
- OpenGL 4.1+ (Core Profile)
- C++17 兼容的编译器

### 平台支持
- Windows 10/11 (仅支持 MSVC)
- macOS 11.0+ (支持 Intel 和 Apple Silicon)

> **注意**: 目前不支持 MinGW 编译器

## 快速开始

### 构建选项

- `BUILD_SHARED_LIBS`: 构建动态库 (默认: ON)
- `PROGRAPHICS_BUILD_EXAMPLES`: 构建示例程序 (默认: ON)

### 安装


## 安装

### 方法 1：使用 CMake FetchContent（推荐）

在你的 CMakeLists.txt 中添加：

```cmake
include(FetchContent)
FetchContent_Declare(
        ProGraphics
        GIT_REPOSITORY https://github.com/yang1206/prographics.git
        GIT_TAG main
)
FetchContent_MakeAvailable(ProGraphics)
```

然后在你的目标中链接：

```cmake
target_link_libraries(YourTarget PRIVATE ProGraphics::ProGraphics)
```

### 方法 2：手动构建和安装

```bash
# Clone 仓库
git clone https://github.com/yang1206/prographics.git
cd prographics

# Windows (MSVC)
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build

# macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

然后在你的 CMakeLists.txt 中：

```cmake
find_package(ProGraphics REQUIRED)
target_link_libraries(YourTarget PRIVATE ProGraphics::ProGraphics)
```


## 使用示例

```cpp
#include <QApplication>
#include <QtGui/QSurfaceFormat>
#include <prographics/charts/coordinate/coordinate3d.h>

int main(int argc, char *argv[]) {
    // 设置 OpenGL 格式
    QSurfaceFormat format;
    format.setVersion(4, 1);  // 必须设置为 4.1
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    
    QApplication app(argc, argv);
    
    // 创建 3D 坐标系窗口
    ProGraphics::Coordinate3D coordinate;
    coordinate.resize(800, 600);
    coordinate.show();
    
    return app.exec();
}
```

## 注意事项

1. OpenGL 配置
    - 必须使用 OpenGL 4.1 Core Profile
    - 在程序启动时设置 QSurfaceFormat

2. 编译器兼容性
    - Windows: 仅支持 MSVC 2019+
    - macOS: 需要 AppleClang 14.0+
    - 不支持 MinGW

3. Qt 版本
    - 推荐使用 Qt 6.5.0 或更高版本
    - 确保 Qt 安装时包含 OpenGL 模块

4. 静态链接注意事项
    - 如果使用静态链接，需要遵循 LGPL 许可证要求
    - 建议优先使用动态链接方式

## 许可证

本项目基于 LGPL-3.0 许可证。详情请参阅 [LICENSE](./LICENSE) 文件。

注意：
- 如果以动态链接方式使用本库，你的应用程序可以使用任何许可证
- 如果以静态链接方式使用本库，你的应用程序必须遵循 LGPL 许可证
- 对本库的任何修改都需要以相同的许可证开源
## 贡献

欢迎提交 Issue 和 Pull Request！

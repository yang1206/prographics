# ProGraphics

ProGraphics 是一个基于 Qt 和 OpenGL 的图形库，提供了简单易用的 3D 图形渲染功能。

[![Windows Build](https://github.com/yang1206/qt-template/actions/workflows/windows-build.yml/badge.svg)](https://github.com/yang1206/prographics/actions/workflows/windows-build.yml)
[![macOS Build](https://github.com/yang1206/qt-template/actions/workflows/macos-build.yml/badge.svg)](https://github.com/yang1206/prographics/actions/workflows/macos-build.yml)

## 特性

- 基于 Qt6 和 OpenGL
- 支持 3D 坐标系统
- 提供基础图形渲染
- 支持纹理加载
- 相机控制系统
- 文本渲染

## 系统要求

- CMake 3.16+
- Qt6
- C++20 编译器
- OpenGL 支持

## 安装

### 方法 1：使用 CMake FetchContent

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

### 方法 2：手动安装

1. 克隆仓库：

```bash
git clone https://github.com/yang1206/prographics.git
```

2. 构建和安装：

```bash
cd prographics
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build
```

3. 在你的 CMakeLists.txt 中添加：

```cmake
find_package(ProGraphics REQUIRED)
target_link_libraries(YourTarget PRIVATE ProGraphics::ProGraphics)
```

## 使用示例

```cpp
#include <QApplication>
#include <prographics/charts/coordinate/coordinate3d.h>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 创建一个 3D 坐标系窗口
    ProGraphics::Coordinate3D coordinate;
    coordinate.resize(800, 600);
    coordinate.show();
    
    return app.exec();
}
```

## 许可证

本项目基于 MIT 许可证。详情请参阅 [LICENSE](./LICENSE) 文件。

## 贡献

欢迎提交 Issue 和 Pull Request！

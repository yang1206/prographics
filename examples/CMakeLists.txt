cmake_minimum_required(VERSION 3.16)

project(ProGraphicsExamples LANGUAGES CXX)

# 设置CMake变量
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找Qt依赖
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)

# 收集所有源文件
file(GLOB_RECURSE EXAMPLE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# 创建可执行文件
qt_add_executable(ProGraphicsExamples
        ${EXAMPLE_SOURCES}
#        resources/shaders.qrc
#        resources/textures.qrc
)

# 链接ProGraphics库
target_link_libraries(ProGraphicsExamples
        PRIVATE
        ProGraphics::ProGraphics
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
)

# 设置包含目录
target_include_directories(ProGraphicsExamples
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 设置编译选项
if(MSVC)
    target_compile_options(ProGraphicsExamples
            PRIVATE
            /W4
            /utf-8
    )
endif()

# 完成Qt设置
qt_finalize_executable(ProGraphicsExamples)
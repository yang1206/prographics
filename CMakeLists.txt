cmake_minimum_required(VERSION 3.16)

project(ProGraphics
        VERSION 1.0.0
        DESCRIPTION "A Qt-based OpenGL graphics library"
        HOMEPAGE_URL "https://github.com/yang1206/prographics"
        LANGUAGES CXX
)

# 设置CMake选项
option(PROGRAPHICS_BUILD_EXAMPLES "Build ProGraphics examples" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

# 设置CMake变量
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找依赖
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)

# 收集源文件
file(GLOB_RECURSE PROGRAPHICS_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/charts/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
)

file(GLOB_RECURSE PROGRAPHICS_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/prographics/core/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/prographics/charts/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/prographics/utils/*.h"
)

# 创建库目标
if(BUILD_SHARED_LIBS)
    add_library(ProGraphics SHARED ${PROGRAPHICS_SOURCES} ${PROGRAPHICS_HEADERS})
    target_compile_definitions(ProGraphics
            PRIVATE PROGRAPHICS_EXPORTS
            PUBLIC PROGRAPHICS_SHARED
    )
else()
    add_library(ProGraphics STATIC ${PROGRAPHICS_SOURCES} ${PROGRAPHICS_HEADERS})
endif()


add_library(ProGraphics::ProGraphics ALIAS ProGraphics)

# 生成导出头文件
include(GenerateExportHeader)
generate_export_header(ProGraphics
        BASE_NAME PROGRAPHICS
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/prographics/prographics_export.h
)

# 设置目标属性
target_include_directories(ProGraphics
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 设置编译选项
target_compile_features(ProGraphics PUBLIC cxx_std_20)

# 编译器特定配置
if(MSVC)
    target_compile_options(ProGraphics
            PRIVATE
            /W4             # 警告级别
            /WX             # 将警告视为错误
            /Zc:__cplusplus # 启用正确的 __cplusplus 宏
            /utf-8          # 使用 UTF-8 编码
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ProGraphics
            PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -fPIC
    )
endif()

# 设置调试信息
target_compile_options(ProGraphics PRIVATE
        $<$<CONFIG:Debug>:
        $<$<CXX_COMPILER_ID:MSVC>:/Zi>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-g>
        >
)

# 链接Qt库
target_link_libraries(ProGraphics
        PUBLIC
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL
        Qt${QT_VERSION_MAJOR}::OpenGLWidgets
)

# 平台特定链接
if(MSVC)
    target_link_libraries(ProGraphics PUBLIC opengl32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(ProGraphics PUBLIC GL)
endif()

# 安装配置
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# 安装头文件
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/prographics
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 安装库文件
install(
        TARGETS ProGraphics
        EXPORT ProGraphicsTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# 导出目标
install(
        EXPORT ProGraphicsTargets
        FILE ProGraphicsTargets.cmake
        NAMESPACE ProGraphics::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ProGraphics
)

# 生成版本文件
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ProGraphicsConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

# 配置Config文件
configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ProGraphicsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ProGraphicsConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ProGraphics
)

# 安装配置文件
install(
        FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ProGraphicsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ProGraphicsConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ProGraphics
)

# 构建示例
if(PROGRAPHICS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
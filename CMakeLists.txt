cmake_minimum_required(VERSION 3.16)

project(ProGraphics
        VERSION 1.0.0
        DESCRIPTION "A Qt-based OpenGL graphics library"
        HOMEPAGE_URL "https://github.com/yang1206/prographics"
        LANGUAGES CXX
)

# 设置CMake选项
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(PROGRAPHICS_BUILD_EXAMPLES "Build ProGraphics examples" ON)

# 设置CMake变量
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找依赖
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGLWidgets OpenGL)

# 收集源文件 - 修改方式收集所有需要的文件
file(GLOB_RECURSE PROGRAPHICS_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE PROGRAPHICS_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/prographics/*.h"
)

# 明确列出带有Q_OBJECT宏的头文件
set(WRAPPED_HEADER_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/prographics/charts/base/gl_widget.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/prographics/utils/orbit_controls.h"
)

# 为带有Q_OBJECT的类创建专用实现文件
set(MOC_IMPLEMENTATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/moc_impl")
file(MAKE_DIRECTORY ${MOC_IMPLEMENTATION_DIR})

set(MOC_SOURCES)
foreach(HEADER_PATH ${WRAPPED_HEADER_FILES})
    get_filename_component(HEADER_NAME ${HEADER_PATH} NAME_WE)
    set(IMPL_FILE "${MOC_IMPLEMENTATION_DIR}/${HEADER_NAME}_moc_impl.cpp")
    file(WRITE ${IMPL_FILE} "#include \"${HEADER_PATH}\"\n")
    list(APPEND MOC_SOURCES ${IMPL_FILE})
endforeach()

# 添加到源文件列表
list(APPEND PROGRAPHICS_SOURCES ${MOC_SOURCES})

# 创建库目标
if (BUILD_SHARED_LIBS)
    add_library(ProGraphics SHARED ${PROGRAPHICS_SOURCES} ${PROGRAPHICS_HEADERS})
    target_compile_definitions(ProGraphics
            PRIVATE
            ProGraphics_EXPORTS  # 确保使用与export.h匹配的宏名称
            PROGRAPHICS_EXPORTS  # 添加这个以确保兼容性
            PUBLIC
            PROGRAPHICS_SHARED
            PUBLIC
            QT_SHARED
    )
    if(MSVC)
        set_target_properties(ProGraphics PROPERTIES
                WINDOWS_EXPORT_ALL_SYMBOLS ON
        )
    endif()
else ()
    add_library(ProGraphics STATIC ${PROGRAPHICS_SOURCES} ${PROGRAPHICS_HEADERS})
endif ()

set_target_properties(ProGraphics PROPERTIES
        AUTOMOC ON
        AUTORCC ON
        AUTOUIC ON
)


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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
)

install(
        FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/prographics/prographics_export.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/prographics
)

# 设置编译选项
target_compile_features(ProGraphics PUBLIC cxx_std_20)

# 编译器特定配置
if (MSVC)
    target_compile_options(ProGraphics
            PRIVATE
            /W4             # 警告级别
            /Zc:__cplusplus # 启用正确的 __cplusplus 宏
            /utf-8          # 使用 UTF-8 编码
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ProGraphics
            PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -fPIC
    )
endif ()

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
if (MSVC)
    target_link_libraries(ProGraphics PUBLIC opengl32)
elseif (UNIX AND NOT APPLE)
    target_link_libraries(ProGraphics PUBLIC GL)
endif ()

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
if (PROGRAPHICS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
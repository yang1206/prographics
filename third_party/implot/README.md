# ImPlot 刻度算法来源

`src/charts/line/linear_ticks_p.h` 的 `niceNumber` 和刻度间隔、像素密度选择逻辑适配自 [ImPlot v1.1](https://github.com/epezent/implot/blob/v1.1/implot.cpp) 的 `NiceNum`、`Locator_Default`。完整 MIT 授权见同目录 `LICENSE`。

Qt 适配使用本库字体尺寸测量标签，按最终间隔统一小数位数，并为极端数值范围增加有限值及重复刻度保护。刻度与网格共用定位结果。

示例的连续完整帧提交顺序参考 [Dear ImGui 官方 OpenGL 示例主循环](https://github.com/ocornut/imgui/blob/master/examples/example_glfw_opengl3/main.cpp)：提交本帧、绘制与交换、再提交下一帧。Qt 的 `frameSwapped` 用于驱动下一帧；默认不限速，用户可以在示例中主动设置生成帧率上限。

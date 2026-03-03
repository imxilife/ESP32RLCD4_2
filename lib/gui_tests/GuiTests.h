#pragma once

#include <Gui.h>

/**
 * GUI 测试函数集合
 * 包含各种图形绘制和文本显示的测试用例
 */
class GuiTests {
public:
    /**
     * 测试基本几何图形：矩形和圆
     * @param gui GUI 实例引用
     */
    static void testBasicShapes(Gui &gui);

    /**
     * 测试圆角矩形
     * @param gui GUI 实例引用
     */
    static void testRoundRect(Gui &gui);

    /**
     * 测试三角形和线段
     * @param gui GUI 实例引用
     */
    static void testTriangleAndLines(Gui &gui);

    /**
     * 测试 ASCII 文本渲染
     * @param gui GUI 实例引用
     */
    static void testAsciiText(Gui &gui);

    /**
     * 测试 UTF-8 文本渲染（含中文）
     * @param gui GUI 实例引用
     */
    static void testUTF8Text(Gui &gui);

    /**
     * 运行所有测试
     * @param gui GUI 实例引用
     */
    static void runAllTests(Gui &gui);
};

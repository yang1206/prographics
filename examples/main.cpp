#include "mainwindow.h"
#include "stress_test_dialog.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QTimer>

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QSurfaceFormat::setDefaultFormat(ProGraphics::chartSurfaceFormat());
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("ProGraphicsExamples");

    QCommandLineParser parser;
    parser.setApplicationDescription("ProGraphics 图表调试工具");
    parser.addHelpOption();
    parser.addOption({"stress", "直接打开性能压力测试窗口。"});
    parser.addOption({"stress-auto", "自动完成压测、保存 CSV 后退出；压力超载仍会保存报告。", "CSV路径"});
    parser.addOption({"stress-preset", "自动压测预设：daily、dense、steady、range。", "预设", "daily"});
    parser.addOption({"stress-warmup", "自动压测预热秒数，范围 1～60。", "秒数", "10"});
    parser.addOption({"stress-duration", "自动压测计时秒数，范围 1～3600。", "秒数", "60"});
    parser.process(a);
    if (parser.isSet("stress") || parser.isSet("stress-auto")) {
        const QStringList presets{"daily", "dense", "steady", "range"};
        const int presetIndex = presets.indexOf(parser.value("stress-preset"));
        bool warmupOk = false, durationOk = false;
        const int warmup = parser.value("stress-warmup").toInt(&warmupOk);
        const int duration = parser.value("stress-duration").toInt(&durationOk);
        if (presetIndex < 0 || !warmupOk || !durationOk || warmup < 1 || warmup > 60 || duration < 1 || duration > 3600
            || (parser.isSet("stress-auto") && parser.value("stress-auto").trimmed().isEmpty())) {
            qCritical("压测参数无效，请使用 --help 查看参数范围。");
            return 2;
        }
        StressTestDialog window;
        window.show();
        if (parser.isSet("stress-auto")) {
            QTimer::singleShot(0, &window, [&window, warmup, duration, presetIndex, &parser] {
                window.startAutomatically(warmup, duration, parser.value("stress-auto"), presetIndex);
            });
        }
        return QApplication::exec();
    }

    MainWindow window;
    window.show();
    return QApplication::exec();
}

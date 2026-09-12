#include <QApplication>

#include "UI/MainWindow/MainWindow.h"

// 程序入口：创建 Qt 应用和主窗口，然后进入事件循环。
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    pms::MainWindow window;
    window.show();
    return app.exec();
}

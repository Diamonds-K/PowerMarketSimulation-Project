#include <QApplication>

#include "UI/MainWindow/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    pms::MainWindow window;
    window.show();
    return app.exec();
}

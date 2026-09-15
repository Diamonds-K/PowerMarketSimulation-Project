#include <QApplication>

#include "UI/LoginDialog/LoginDialog.h"
#include "UI/MainWindow/MainWindow.h"

// 程序入口：登录验证通过后再创建主窗口并进入事件循环。
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    pms::LoginDialog loginDialog;
    if (loginDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    pms::MainWindow window;
    window.show();
    return app.exec();
}

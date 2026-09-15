#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace pms {

// 启动登录窗口，验证管理员凭据后才允许创建主窗口。
class LoginDialog : public QDialog {
    Q_OBJECT

public:
    // 创建登录窗口及用户名、密码输入控件。
    explicit LoginDialog(QWidget* parent = nullptr);

private slots:
    // 校验当前输入；成功则接受对话框，失败则清空密码并提示。
    void attemptLogin();

private:
    QLineEdit* usernameEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QPushButton* loginButton_ = nullptr;
    QPushButton* exitButton_ = nullptr;
};

} // namespace pms

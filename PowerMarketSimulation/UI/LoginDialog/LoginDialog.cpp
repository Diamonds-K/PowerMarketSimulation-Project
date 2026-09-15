#include "UI/LoginDialog/LoginDialog.h"

#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace pms {

namespace {

const QString kAdministratorUsername = QStringLiteral("administrator");
const QString kAdministratorPassword = QStringLiteral("111111");

} // namespace

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("系统登录"));
    setFixedSize(420, 260);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(32, 24, 32, 24);
    rootLayout->setSpacing(16);

    auto* titleLabel = new QLabel(QStringLiteral("电力现货市场出清仿真平台"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(titleLabel);

    auto* formLayout = new QFormLayout;
    formLayout->setHorizontalSpacing(16);
    formLayout->setVerticalSpacing(14);

    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setMinimumHeight(32);
    usernameEdit_->setPlaceholderText(QStringLiteral("请输入用户名"));

    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setMinimumHeight(32);
    passwordEdit_->setPlaceholderText(QStringLiteral("请输入密码"));
    passwordEdit_->setEchoMode(QLineEdit::Password);

    formLayout->addRow(QStringLiteral("用户名："), usernameEdit_);
    formLayout->addRow(QStringLiteral("密码："), passwordEdit_);
    rootLayout->addLayout(formLayout);

    errorLabel_ = new QLabel(QStringLiteral("用户名或密码错误，请重新输入"), this);
    errorLabel_->setStyleSheet(QStringLiteral("color: #b00020;"));
    errorLabel_->setWordWrap(true);
    errorLabel_->setVisible(false);
    rootLayout->addWidget(errorLabel_);
    rootLayout->addStretch();

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    loginButton_ = new QPushButton(QStringLiteral("登录"), this);
    loginButton_->setDefault(true);
    loginButton_->setMinimumWidth(88);
    buttonLayout->addWidget(loginButton_);

    exitButton_ = new QPushButton(QStringLiteral("退出"), this);
    exitButton_->setMinimumWidth(88);
    buttonLayout->addWidget(exitButton_);
    rootLayout->addLayout(buttonLayout);

    connect(usernameEdit_, &QLineEdit::returnPressed,
            passwordEdit_, qOverload<>(&QWidget::setFocus));
    connect(passwordEdit_, &QLineEdit::returnPressed,
            this, &LoginDialog::attemptLogin);
    connect(loginButton_, &QPushButton::clicked,
            this, &LoginDialog::attemptLogin);
    connect(exitButton_, &QPushButton::clicked,
            this, &QDialog::reject);

    usernameEdit_->setFocus();
}

void LoginDialog::attemptLogin()
{
    if (usernameEdit_->text().trimmed() == kAdministratorUsername &&
        passwordEdit_->text() == kAdministratorPassword) {
        accept();
        return;
    }

    errorLabel_->setVisible(true);
    passwordEdit_->clear();
    passwordEdit_->setFocus();
}

} // namespace pms

#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace pms {

class LoginDialog : public QDialog {
    Q_OBJECT

public:
    explicit LoginDialog(QWidget* parent = nullptr);

private slots:
    void attemptLogin();

private:
    QLineEdit* usernameEdit_ = nullptr;
    QLineEdit* passwordEdit_ = nullptr;
    QLabel* errorLabel_ = nullptr;
    QPushButton* loginButton_ = nullptr;
    QPushButton* exitButton_ = nullptr;
};

} // namespace pms

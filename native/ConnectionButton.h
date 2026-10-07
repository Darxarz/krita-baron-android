// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QPushButton>
class ConnectionButton : public QPushButton {
    Q_OBJECT
public:
    ConnectionButton(const QString& title, QWidget* parent = nullptr);
    void setStatus(const QString& text, const QColor& color);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString m_status;
    QColor m_color;
};

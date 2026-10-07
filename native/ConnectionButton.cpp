// SPDX-License-Identifier: GPL-3.0-or-later
// Native reconstruction of Acly's ServerModeButton.
#include "ConnectionButton.h"
#include <QCoreApplication>
#include <QPainter>
#include <QStyleOption>
ConnectionButton::ConnectionButton(const QString& title, QWidget* parent) : QPushButton(title, parent) {
    setCheckable(true);
    setMinimumWidth(fontMetrics().horizontalAdvance(title) + 120);
    setFixedHeight(int(1.3 * QPushButton::sizeHint().height()));
    setStatus(QCoreApplication::translate("BaronPanel", "Not connected"), QColor("#888888"));
}
void ConnectionButton::setStatus(const QString& text, const QColor& color) { m_status = text; m_color = color; update(); }
void ConnectionButton::paintEvent(QPaintEvent*) {
    QStyleOption option; option.initFrom(this);
    if (isChecked()) option.state |= QStyle::State_Sunken;
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &option, &painter, this);
    const auto area = rect().adjusted(8, 0, -8, 0);
    auto bold = font(); bold.setBold(true); painter.setFont(bold);
    painter.drawText(area, Qt::AlignLeft | Qt::AlignVCenter, text());
    painter.setFont(font()); painter.setPen(isEnabled() ? m_color : palette().color(QPalette::Disabled, QPalette::Text));
    painter.drawText(area, Qt::AlignRight | Qt::AlignVCenter, m_status);
}

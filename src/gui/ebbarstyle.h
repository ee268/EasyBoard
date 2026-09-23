#ifndef EBBARSTYLE_H
#define EBBARSTYLE_H

#include <QToolBar>

inline void ebStyleBar(QToolBar *bar, const QString &border)
{
    bar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    bar->setIconSize(QSize(24, 24));
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #F8FAFB; border: none; %1 padding: 5px 8px; spacing: 2px; }"
        "QToolBar QToolButton { background: transparent; border: 1px solid transparent; border-radius: 8px; min-width: 34px; min-height: 34px; padding: 3px; }"
        "QToolBar QToolButton:hover { background: #EAF4F2; }"
        "QToolBar QToolButton:checked { background: #DDF3EF; border-color: #A5DDD4; }"
        "QToolBar QToolButton:pressed { background: #C7E9E2; }"
        "QToolBar::separator { background: #D8E2E6; width: 1px; height: 1px; margin: 7px 6px; }").arg(border));
}

#endif

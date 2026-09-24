#include "ebbarstyle.h"


void ebStyleBar(QToolBar *bar, const QString &border, bool showLabels)
{
    bar->setToolButtonStyle(showLabels ? Qt::ToolButtonTextUnderIcon
                                       : Qt::ToolButtonIconOnly);
    bar->setIconSize(QSize(24, 24));
    bar->setMovable(false);
    bar->setFloatable(false);
    const QString buttonSize = showLabels
        ? QStringLiteral("min-width: 58px; min-height: 64px; padding: 4px 7px; font-size: 14px;")
        : QStringLiteral("min-width: 34px; min-height: 34px; padding: 3px;");
    bar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #F8FAFB; border: none; %1 padding: 5px 8px; spacing: 2px; }"
        "QToolBar > QToolButton { background: transparent; border: 1px solid transparent; border-radius: 8px; %2 }"
        "QToolBar > QToolButton:hover { background: #EAF4F2; }"
        "QToolBar > QToolButton:checked { background: #DDF3EF; border-color: #A5DDD4; }"
        "QToolBar > QToolButton:pressed { background: #C7E9E2; }"
        "QToolBar::separator { background: #D8E2E6; width: 1px; height: 1px; margin: 7px 6px; }").arg(border, buttonSize));
}

#include "ebdialoglocalizer.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QEvent>
#include <QTimer>

namespace {
QString buttonText(QDialogButtonBox::StandardButton button)
{
    switch (button) {
    case QDialogButtonBox::Ok: return QStringLiteral("确定");
    case QDialogButtonBox::Open: return QStringLiteral("打开");
    case QDialogButtonBox::Save: return QStringLiteral("保存");
    case QDialogButtonBox::Cancel: return QStringLiteral("取消");
    case QDialogButtonBox::Close: return QStringLiteral("关闭");
    case QDialogButtonBox::Discard: return QStringLiteral("放弃");
    case QDialogButtonBox::Apply: return QStringLiteral("应用");
    case QDialogButtonBox::Reset: return QStringLiteral("重置");
    case QDialogButtonBox::RestoreDefaults: return QStringLiteral("恢复默认");
    case QDialogButtonBox::Help: return QStringLiteral("帮助");
    case QDialogButtonBox::SaveAll: return QStringLiteral("全部保存");
    case QDialogButtonBox::Yes: return QStringLiteral("是");
    case QDialogButtonBox::YesToAll: return QStringLiteral("全部是");
    case QDialogButtonBox::No: return QStringLiteral("否");
    case QDialogButtonBox::NoToAll: return QStringLiteral("全部否");
    case QDialogButtonBox::Abort: return QStringLiteral("中止");
    case QDialogButtonBox::Retry: return QStringLiteral("重试");
    case QDialogButtonBox::Ignore: return QStringLiteral("忽略");
    default: return QString();
    }
}
}

EBDialogLocalizer::EBDialogLocalizer(QObject *parent)
    : QObject(parent)
{
}

bool EBDialogLocalizer::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Show) {
        QDialogButtonBox *box = qobject_cast<QDialogButtonBox *>(object);
        if (box) {
            QTimer::singleShot(0, box, [box]() {
                localizeButtons(box);
            });
        }
    }
    return false;
}

void EBDialogLocalizer::localizeButtons(QDialogButtonBox *box)
{
    for (QAbstractButton *button : box->buttons()) {
        const QString text = buttonText(box->standardButton(button));
        if (!text.isEmpty())
            button->setText(text);
    }
}

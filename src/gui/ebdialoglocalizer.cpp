#include "ebdialoglocalizer.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QEvent>
#include <QTimer>

namespace {
QString buttonText(QDialogButtonBox::StandardButton button, bool english)
{
    if (english) {
        switch (button) {
        case QDialogButtonBox::Ok: return QStringLiteral("OK");
        case QDialogButtonBox::Open: return QStringLiteral("Open");
        case QDialogButtonBox::Save: return QStringLiteral("Save");
        case QDialogButtonBox::Cancel: return QStringLiteral("Cancel");
        case QDialogButtonBox::Close: return QStringLiteral("Close");
        case QDialogButtonBox::Discard: return QStringLiteral("Discard");
        case QDialogButtonBox::Apply: return QStringLiteral("Apply");
        case QDialogButtonBox::Reset: return QStringLiteral("Reset");
        case QDialogButtonBox::RestoreDefaults: return QStringLiteral("Restore Defaults");
        case QDialogButtonBox::Help: return QStringLiteral("Help");
        case QDialogButtonBox::SaveAll: return QStringLiteral("Save All");
        case QDialogButtonBox::Yes: return QStringLiteral("Yes");
        case QDialogButtonBox::YesToAll: return QStringLiteral("Yes to All");
        case QDialogButtonBox::No: return QStringLiteral("No");
        case QDialogButtonBox::NoToAll: return QStringLiteral("No to All");
        case QDialogButtonBox::Abort: return QStringLiteral("Abort");
        case QDialogButtonBox::Retry: return QStringLiteral("Retry");
        case QDialogButtonBox::Ignore: return QStringLiteral("Ignore");
        default: return QString();
        }
    }
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

EBDialogLocalizer::EBDialogLocalizer(bool english, QObject *parent)
    : QObject(parent)
    , _english(english)
{
}

bool EBDialogLocalizer::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Show) {
        QDialogButtonBox *box = qobject_cast<QDialogButtonBox *>(object);
        if (box) {
            QTimer::singleShot(0, box, [box, english = _english]() {
                localizeButtons(box, english);
            });
        }
    }
    return false;
}

void EBDialogLocalizer::localizeButtons(QDialogButtonBox *box, bool english)
{
    for (QAbstractButton *button : box->buttons()) {
        const QString text = buttonText(box->standardButton(button), english);
        if (!text.isEmpty())
            button->setText(text);
    }
}

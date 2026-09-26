#ifndef EBDIALOGLOCALIZER_H
#define EBDIALOGLOCALIZER_H

#include <QObject>

class QDialogButtonBox;
class QEvent;

// Qt 的 Windows 平台主题会覆盖标准按钮文本；显示时统一设置中文标签。
class EBDialogLocalizer : public QObject
{
public:
    explicit EBDialogLocalizer(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    static void localizeButtons(QDialogButtonBox *box);
};

#endif // EBDIALOGLOCALIZER_H

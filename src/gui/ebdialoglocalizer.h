#ifndef EBDIALOGLOCALIZER_H
#define EBDIALOGLOCALIZER_H

#include <QObject>

class QDialogButtonBox;
class QEvent;

// Qt 的 Windows 平台主题会覆盖标准按钮文本；显示时按当前界面语言设置标签。
class EBDialogLocalizer : public QObject
{
public:
    explicit EBDialogLocalizer(bool english = false, QObject *parent = nullptr);
    void setEnglish(bool english);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    static void localizeButtons(QDialogButtonBox *box, bool english);

    bool _english;
};

#endif // EBDIALOGLOCALIZER_H

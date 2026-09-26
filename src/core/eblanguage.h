#ifndef EBLANGUAGE_H
#define EBLANGUAGE_H

#include <QObject>
#include <QTranslator>

// 应用只保留一组活动翻译器，语言变化后由各界面刷新已有动作。
class EBLanguage : public QObject
{
    Q_OBJECT
public:
    explicit EBLanguage(QObject *parent = nullptr);
    static EBLanguage *instance();
    QString language() const;
    bool setLanguage(const QString &language);

signals:
    void languageChanged();

private:
    static EBLanguage *_instance;
    QTranslator _qtTranslator;
    QTranslator _englishTranslator;
    QString _language;
};

#endif // EBLANGUAGE_H

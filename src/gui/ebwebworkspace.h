#ifndef EBWEBWORKSPACE_H
#define EBWEBWORKSPACE_H

#include <QImage>
#include <QPointer>
#include <QUrl>
#include <QWidget>

class QAction;
class QLineEdit;
class QTabWidget;
class QWebEngineView;
class QWebEngineProfile;
class EBWebCapture;
class EBWebHistory;

// 网页模式只负责浏览和可见区域截图，不直接修改白板文档。
class EBWebWorkspace : public QWidget
{
    Q_OBJECT
public:
    explicit EBWebWorkspace(QWidget *parent = nullptr);
    ~EBWebWorkspace() override;

    QWebEngineView *createTab(const QUrl &url = QUrl());
    QWebEngineView *currentView() const;
    QWebEngineProfile *profile() const;
    int tabCount() const;
    void navigate(const QString &address);
    void startCapture();

signals:
    void imageCaptured(const QImage &image);
    void statusMessage(const QString &message);

private:
    void closeTab(int index);
    void refreshNavigation();
    void scheduleSessionSave();
    void saveSession() const;

    QTabWidget *_tabs;
    QWebEngineProfile *_profile;
    QLineEdit *_address;
    EBWebHistory *_history;
    QAction *_backAction;
    QAction *_forwardAction;
    QAction *_reloadAction;
    QAction *_externalAction;
    QAction *_captureAction;
    QPointer<EBWebCapture> _capture;
    class QTimer *_sessionTimer;
    bool _restoringSession = false;
};

#endif

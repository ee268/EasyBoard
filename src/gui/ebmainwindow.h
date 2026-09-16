#ifndef EBMAINWINDOW_H
#define EBMAINWINDOW_H

#include <QMainWindow>

class EBMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EBMainWindow(const QString& fileToOpen, QWidget *parent = nullptr);
};

#endif // EBMAINWINDOW_H

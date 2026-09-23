#include "ebcommands.h"

#include "ebicons.h"

#include <QAction>
#include <QFileDialog>
#include <QIcon>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

#include "../board/ebboardview.h"
#include "../core/ebsettings.h"
#include "../import/ebimageimporter.h"
#include "../persistence/ebdocumentpackage.h"

void EBCommands::createFileMenu()
{
    QMenu *fileMenu = _window->menuBar()->addMenu(tr("文件(&F)"));
    QAction *newAction = fileMenu->addAction(ebToolbarIcon("file_new"),
                                              tr("新建文档"));
    newAction->setObjectName(QStringLiteral("newDocumentAction"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered,
            this, &EBCommands::newDocumentRequested);
    fileMenu->addSeparator();
    QAction *openAction = fileMenu->addAction(ebToolbarIcon("file_open"),
                                               tr("打开文档..."));
    openAction->setObjectName(QStringLiteral("openFileAction"));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("打开 EasyBoard 文档"), QString(),
            tr("EasyBoard 文档 (*.json)"));
        if (!path.isEmpty())
            emit fileImportRequested(path);
    });

    QAction *importAction = fileMenu->addAction(
        ebToolbarIcon("file_import_image"), tr("导入图片为新文档..."));
    importAction->setObjectName(QStringLiteral("importImageAction"));
    importAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(importAction, &QAction::triggered, this, [this]() {
        // 菜单只收集图片路径，应用层统一处理菜单和实例间请求。
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("导入图片为新文档"), QString(),
            EBImageImporter::fileDialogFilter());
        if (!path.isEmpty())
            emit fileImportRequested(path);
    });

    _insertImageObjectAction = fileMenu->addAction(tr("插入图片对象..."));
    _insertImageObjectAction->setObjectName(
        QStringLiteral("insertImageObjectAction"));
    _insertImageObjectAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    connect(_insertImageObjectAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("插入图片对象"), QString(),
            EBImageImporter::fileDialogFilter());
        if (!path.isEmpty())
            emit imageObjectInsertRequested(path);
    });

    QAction *importPackageAction = fileMenu->addAction(
        ebToolbarIcon("file_package_import"), tr("导入课程文档包..."));
    importPackageAction->setObjectName(
        QStringLiteral("importDocumentPackageAction"));
    connect(importPackageAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            _window, tr("导入 EasyBoard 课程文档包"), QString(),
            EBDocumentPackage::fileDialogFilter());
        if (!path.isEmpty())
            emit fileImportRequested(path);
    });

    QAction *saveAction = fileMenu->addAction(ebToolbarIcon("file_save"),
                                               tr("保存"));
    saveAction->setObjectName(QStringLiteral("saveDocumentAction"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered,
            this, &EBCommands::saveDocumentRequested);

    fileMenu->addSeparator();
    QAction *exportPageAction = fileMenu->addAction(
        ebToolbarIcon("file_export_image"), tr("导出当前页图片..."));
    exportPageAction->setObjectName(QStringLiteral("exportPageImageAction"));
    connect(exportPageAction, &QAction::triggered,
            this, &EBCommands::exportPageImageRequested);
    QAction *exportPdfAction = fileMenu->addAction(
        ebToolbarIcon("file_export_pdf"), tr("导出整份文档 PDF..."));
    exportPdfAction->setObjectName(QStringLiteral("exportDocumentPdfAction"));
    connect(exportPdfAction, &QAction::triggered,
            this, &EBCommands::exportDocumentPdfRequested);
    QAction *exportPackageAction = fileMenu->addAction(
        ebToolbarIcon("file_package_export"), tr("导出课程文档包..."));
    exportPackageAction->setObjectName(
        QStringLiteral("exportDocumentPackageAction"));
    connect(exportPackageAction, &QAction::triggered,
            this, &EBCommands::exportDocumentPackageRequested);

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(ebToolbarIcon("file_exit"),
                                               tr("退出"));
    quitAction->setObjectName(QStringLiteral("quitAction"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &EBCommands::quitRequested);
}

void EBCommands::createEditMenu()
{
    QMenu *editMenu = _window->menuBar()->addMenu(tr("编辑(&E)"));
    _cutAction = editMenu->addAction(tr("剪切"));
    _cutAction->setObjectName(QStringLiteral("cutObjectAction"));
    _cutAction->setShortcut(QKeySequence::Cut);
    _copyAction = editMenu->addAction(tr("复制"));
    _copyAction->setObjectName(QStringLiteral("copyObjectAction"));
    _copyAction->setShortcut(QKeySequence::Copy);
    _pasteAction = editMenu->addAction(tr("粘贴"));
    _pasteAction->setObjectName(QStringLiteral("pasteObjectAction"));
    _pasteAction->setShortcut(QKeySequence::Paste);
    _cutAction->setEnabled(false);
    _copyAction->setEnabled(false);
    _pasteAction->setEnabled(false);
    connect(_cutAction, &QAction::triggered,
            _boardView, &EBBoardView::cutSelectedObject);
    connect(_copyAction, &QAction::triggered,
            _boardView, &EBBoardView::copySelectedObject);
    connect(_pasteAction, &QAction::triggered,
            _boardView, &EBBoardView::pasteObject);
    _duplicateAction = editMenu->addAction(ebToolbarIcon("duplicate"),
                                            tr("快速复制"));
    _duplicateAction->setObjectName(QStringLiteral("duplicateObjectsAction"));
    _duplicateAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    _duplicateAction->setEnabled(false);
    connect(_duplicateAction, &QAction::triggered,
            _boardView, &EBBoardView::duplicateSelectedObjects);
    _selectAllAction = editMenu->addAction(ebToolbarIcon("select_all"),
                                            tr("全选"));
    _selectAllAction->setObjectName(QStringLiteral("selectAllObjectsAction"));
    _selectAllAction->setShortcut(QKeySequence::SelectAll);
    _selectAllAction->setEnabled(false);
    connect(_selectAllAction, &QAction::triggered,
            _boardView, &EBBoardView::selectAllObjects);

    editMenu->addSeparator();
    _groupAction = editMenu->addAction(tr("组合"));
    _groupAction->setObjectName(QStringLiteral("groupObjectsAction"));
    _groupAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    _groupAction->setEnabled(false);
    _ungroupAction = editMenu->addAction(tr("取消组合"));
    _ungroupAction->setObjectName(QStringLiteral("ungroupObjectsAction"));
    _ungroupAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    _ungroupAction->setEnabled(false);
    connect(_groupAction, &QAction::triggered,
            _boardView, &EBBoardView::groupSelectedObjects);
    connect(_ungroupAction, &QAction::triggered,
            _boardView, &EBBoardView::ungroupSelectedObjects);

    _lockAction = editMenu->addAction(ebToolbarIcon("object_lock"),
                                      tr("锁定对象"));
    _lockAction->setObjectName(QStringLiteral("lockObjectsAction"));
    _lockAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    _lockAction->setEnabled(false);
    _unlockAction = editMenu->addAction(ebToolbarIcon("object_unlock"),
                                        tr("解锁对象"));
    _unlockAction->setObjectName(QStringLiteral("unlockObjectsAction"));
    _unlockAction->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    _unlockAction->setEnabled(false);
    connect(_lockAction, &QAction::triggered,
            _boardView, &EBBoardView::lockSelectedObjects);
    connect(_unlockAction, &QAction::triggered,
            _boardView, &EBBoardView::unlockSelectedObjects);

    QMenu *arrangeMenu = editMenu->addMenu(ebToolbarIcon("object_arrange"),
                                            tr("对齐与分布"));
    const QString arrangeLabels[] = {
        tr("左对齐"), tr("水平居中"), tr("右对齐"),
        tr("顶部对齐"), tr("垂直居中"), tr("底部对齐"),
        tr("水平等距分布"), tr("垂直等距分布")
    };
    const char *arrangeNames[] = {
        "alignObjectsLeftAction", "alignObjectsHorizontalCenterAction",
        "alignObjectsRightAction", "alignObjectsTopAction",
        "alignObjectsVerticalCenterAction", "alignObjectsBottomAction",
        "distributeObjectsHorizontalAction", "distributeObjectsVerticalAction"
    };
    const char *arrangeIcons[] = {
        "object_align_left", "object_align_hcenter", "object_align_right",
        "object_align_top", "object_align_vcenter", "object_align_bottom",
        "object_distribute_horizontal", "object_distribute_vertical"
    };
    for (int index = 0; index < 8; ++index) {
        if (index == 3 || index == 6)
            arrangeMenu->addSeparator();
        QAction *action = arrangeMenu->addAction(
            ebToolbarIcon(arrangeIcons[index]), arrangeLabels[index]);
        action->setObjectName(QString::fromLatin1(arrangeNames[index]));
        action->setEnabled(false);
        _arrangeActions[index] = action;
        connect(action, &QAction::triggered, _boardView,
                [this, index]() {
            _boardView->arrangeSelectedObjects(
                static_cast<EBBoardView::ObjectArrangement>(index));
        });
    }

    editMenu->addSeparator();
    QMenu *snapMenu = editMenu->addMenu(tr("吸附"));
    _snapAction = snapMenu->addAction(tr("启用吸附"));
    _snapAction->setObjectName(QStringLiteral("snapObjectsAction"));
    _snapAction->setCheckable(true);
    _snapAction->setChecked(EBSettings::settings()->snapEnabled());
    _boardView->setSnapEnabled(_snapAction->isChecked());
    _gridSnapAction = snapMenu->addAction(tr("吸附到网格"));
    _gridSnapAction->setObjectName(QStringLiteral("snapGridAction"));
    _gridSnapAction->setToolTip(tr("仅在当前页面使用网格底纹时生效"));
    _gridSnapAction->setCheckable(true);
    _gridSnapAction->setChecked(EBSettings::settings()->gridSnapEnabled());
    _boardView->setGridSnapEnabled(_gridSnapAction->isChecked());
    connect(_snapAction, &QAction::toggled, this, [this](bool enabled) {
        _boardView->setSnapEnabled(enabled);
        EBSettings::settings()->setSnapEnabled(enabled);
        EBSettings::settings()->save();
    });
    connect(_gridSnapAction, &QAction::toggled, this, [this](bool enabled) {
        _boardView->setGridSnapEnabled(enabled);
        EBSettings::settings()->setGridSnapEnabled(enabled);
        EBSettings::settings()->save();
    });

    editMenu->addSeparator();
    const QString labels[] = {tr("置于底层"), tr("下移一层"),
                              tr("上移一层"), tr("置于顶层")};
    const char *names[] = {"sendObjectToBackAction", "moveObjectBackwardAction",
                           "moveObjectForwardAction", "bringObjectToFrontAction"};
    for (int index = 0; index < 4; ++index) {
        _layerActions[index] = editMenu->addAction(labels[index]);
        _layerActions[index]->setObjectName(QString::fromLatin1(names[index]));
        _layerActions[index]->setEnabled(false);
    }
    _layerActions[0]->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft));
    _layerActions[1]->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    _layerActions[2]->setShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    _layerActions[3]->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
    connect(_layerActions[0], &QAction::triggered,
            _boardView, &EBBoardView::sendSelectedObjectToBack);
    connect(_layerActions[1], &QAction::triggered,
            _boardView, &EBBoardView::moveSelectedObjectBackward);
    connect(_layerActions[2], &QAction::triggered,
            _boardView, &EBBoardView::moveSelectedObjectForward);
    connect(_layerActions[3], &QAction::triggered,
            _boardView, &EBBoardView::bringSelectedObjectToFront);
}

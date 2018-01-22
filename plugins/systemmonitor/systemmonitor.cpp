/*
 * Copyright (C) 2013 ~ 2018 National University of Defense Technology(NUDT) & Tianjin Kylin Ltd.
 *
 * Authors:
 *  Kobe Lee    xiangli@ubuntukylin.com/kobe24_lixiang@126.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "systemmonitor.h"

#include "util.h"
#include <QFileSystemWatcher>
#include <QLabel>
#include <QDebug>
#include <QMouseEvent>
#include <QDesktopWidget>
#include <QFile>
#include <QHBoxLayout>
#include <QApplication>
#include <QScreen>

SystemMonitor::SystemMonitor(QWidget *parent)
    : QFrame(parent)
    , mousePressed(false)
{
    /*this->setAutoFillBackground(true);
    QPalette palette;
    palette.setColor(QPalette::Background, QColor("#0d87ca"));
    this->setPalette(palette);*/

//    this->setStyleSheet("QFrame{border: 1px solid #121212;border-radius:1px;background-color:#1f1f1f;}");
//    this->setAttribute(Qt::WA_DeleteOnClose);


    this->setWindowFlags(this->windowFlags() | Qt::FramelessWindowHint  | Qt::WindowCloseButtonHint);//去掉边框
    this->setAttribute(Qt::WA_TranslucentBackground);//背景透明

//    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setAutoFillBackground(true);
    this->setMouseTracking(true);
//    installEventFilter(this);

    this->resize(900, 600);
    setMinimumSize(640, 480);

    proSettings = new QSettings(KYLIN_COMPANY_SETTING, KYLIN_SETTING_FILE_NAME_SETTING);
    proSettings->setIniCodec("UTF-8");

    this->initTitleWidget();
    this->initToolBar();
    this->initPanelStack();
    this->initConnections();

    this->moveCenter();
}

SystemMonitor::~SystemMonitor()
{
    if (m_sysMonitorStack) {
        foreach (QObject *child, m_sysMonitorStack->children()) {
            QWidget *widget = static_cast<QWidget *>(child);
            widget->deleteLater();
        }
        delete m_sysMonitorStack;
    }
    if (m_titleWidget) {
        delete m_titleWidget;
        m_titleWidget = nullptr;
    }
    if (m_toolBar) {
        delete m_toolBar;
        m_toolBar = nullptr;
    }
    if (process_dialog) {
        delete process_dialog;
        process_dialog = nullptr;
    }
    if (resources_dialog) {
        delete resources_dialog;
        resources_dialog = nullptr;
    }
    if (filesystem_dialog) {
        delete filesystem_dialog;
        filesystem_dialog = nullptr;
    }
    if (proSettings != NULL) {
        delete proSettings;
        proSettings = NULL;
    }
}

void SystemMonitor::resizeEvent(QResizeEvent *e)
{
    if (m_titleWidget) {
        m_titleWidget->resize(width(), TOP_TITLE_WIDGET_HEIGHT);
        if (e->oldSize()  != e->size()) {
            emit m_titleWidget->updateMaxBtn();
        }
    }
    if (m_toolBar) {
        m_toolBar->resize(width(), TOP_TITLE_WIDGET_HEIGHT);
        m_toolBar->move(0, TOP_TITLE_WIDGET_HEIGHT);
    }
    if (m_sysMonitorStack) {
        m_sysMonitorStack->resize(width(), this->height() - TOP_TITLE_WIDGET_HEIGHT*2);
        m_sysMonitorStack->move(0, TOP_TITLE_WIDGET_HEIGHT*2);
    }
}

void SystemMonitor::recordVisibleColumn(int, bool, QList<bool> columnVisible)
{
    QList<QString> m_visibleColumns;
    m_visibleColumns << "name";

    if (columnVisible[1]) {
        m_visibleColumns << "user";
    }

    if (columnVisible[2]) {
        m_visibleColumns << "status";
    }

    if (columnVisible[3]) {
        m_visibleColumns << "cpu";
    }

    if (columnVisible[4]) {
        m_visibleColumns << "pid";
    }

    if (columnVisible[5]) {
        m_visibleColumns << "command";
    }

    if (columnVisible[6]) {
        m_visibleColumns << "memory";
    }

    if (columnVisible[7]) {
        m_visibleColumns << "priority";
    }

    QString processColumns = "";
    for (int i = 0; i < m_visibleColumns.length(); i++) {
        if (i != m_visibleColumns.length() - 1) {
            processColumns += QString("%1,").arg(m_visibleColumns[i]);
        } else {
            processColumns += m_visibleColumns[i];
        }
    }

    proSettings->beginGroup("PROCESS");
    proSettings->setValue("ProcessDisplayColumns", processColumns);
    proSettings->endGroup();
    proSettings->sync();
}

void SystemMonitor::recordSortStatus(int index, bool sortOrder)
{
    QList<QString> columnNames = { "name", "user", "status", "cpu", "pid", "command", "memory", "priority"};

    proSettings->beginGroup("PROCESS");
    proSettings->setValue("ProcessCurrentSortColumn", columnNames[index]);
    proSettings->setValue("ProcessSortOrder", sortOrder);
    proSettings->endGroup();
    proSettings->sync();
}

void SystemMonitor::initPanelStack()
{
    m_sysMonitorStack = new QStackedWidget(this);
    m_sysMonitorStack->setStyleSheet("QStackedWidget{background: rgb(255, 255, 255);}");
    m_sysMonitorStack->setObjectName("SystemMonitorStack");
    m_sysMonitorStack->resize(width(), this->height() - TOP_TITLE_WIDGET_HEIGHT);
    m_sysMonitorStack->move(0, TOP_TITLE_WIDGET_HEIGHT);

    m_sysMonitorStack->setMouseTracking(false);
    m_sysMonitorStack->installEventFilter(this);

    process_dialog = new ProcessDialog(getColumnHideFlags(), getSortIndex(), getSortOrder(), proSettings);
    process_dialog->getProcessView()->installEventFilter(this);
    connect(process_dialog, &ProcessDialog::changeColumnVisible, this, &SystemMonitor::recordVisibleColumn);
    connect(process_dialog, &ProcessDialog::changeSortStatus, this, &SystemMonitor::recordSortStatus);
    connect(m_toolBar, SIGNAL(activeWhoseProcessList(int)), process_dialog, SLOT(onActiveWhoseProcess(int)));

    resources_dialog = new ResouresDialog;
    filesystem_dialog = new FileSystemDialog;

    m_sysMonitorStack->addWidget(process_dialog);
    m_sysMonitorStack->addWidget(resources_dialog);
    m_sysMonitorStack->addWidget(filesystem_dialog);
    m_sysMonitorStack->setCurrentWidget(process_dialog);
}

void SystemMonitor::initTitleWidget()
{
    m_titleWidget = new MonitorTitleWidget(this);
    m_titleWidget->resize(width(), TOP_TITLE_WIDGET_HEIGHT);
    m_titleWidget->move(0, 0);
}

void SystemMonitor::initToolBar()
{
    m_toolBar = new ToolBar(proSettings, this);
    m_toolBar->resize(width(), TOP_TITLE_WIDGET_HEIGHT);
    m_toolBar->move(0, TOP_TITLE_WIDGET_HEIGHT);
}

void SystemMonitor::initConnections()
{
    connect(m_toolBar, SIGNAL(changePage(int)), this, SLOT(onChangePage(int)));
    connect(m_toolBar, SIGNAL(canelSearchEditFocus()), process_dialog, SLOT(focusProcessView()));
    connect(m_toolBar, SIGNAL(searchSignal(QString)), process_dialog, SLOT(onSearch(QString)), Qt::QueuedConnection);
}

void SystemMonitor::onChangePage(int index)
{
    if (m_sysMonitorStack) {
        m_sysMonitorStack->setCurrentIndex(index);
        if (index == 1) {
            //start time
            resources_dialog->startCpuTimer();
        }
        else {
            //stop time
            resources_dialog->stopCpuTimer();
        }
    }
}

int SystemMonitor::getSortIndex()
{
    proSettings->beginGroup("PROCESS");
    QString sortingName = proSettings->value("ProcessCurrentSortColumn").toString();
    proSettings->endGroup();

    QList<QString> columnNames = {
        "name", "user", "status", "cpu", "pid", "command", "memory", "priority"
    };

    return columnNames.indexOf(sortingName);
}

bool SystemMonitor::getSortOrder()
{
    proSettings->beginGroup("PROCESS");
    bool value = proSettings->value("ProcessSortOrder", true).toBool();
    proSettings->endGroup();

    return value;
}

QList<bool> SystemMonitor::getColumnHideFlags()
{
    proSettings->beginGroup("PROCESS");
    QString processColumns = proSettings->value("ProcessDisplayColumns", "name,user,status,cpu,pid,command,memory,priority").toString();
    proSettings->endGroup();

    if (processColumns.isEmpty()) {
        proSettings->beginGroup("PROCESS");
        processColumns = "name,user,status,cpu,pid,command,memory,priority";
        proSettings->setValue("ProcessDisplayColumns", processColumns);
        proSettings->endGroup();
        proSettings->sync();
    }

    QList<bool> toggleHideFlags;
    toggleHideFlags << processColumns.contains("name");
    toggleHideFlags << processColumns.contains("user");
    toggleHideFlags << processColumns.contains("status");
    toggleHideFlags << processColumns.contains("cpu");
    toggleHideFlags << processColumns.contains("pid");
    toggleHideFlags << processColumns.contains("command");
    toggleHideFlags << processColumns.contains("memory");
    toggleHideFlags << processColumns.contains("priority");

    return toggleHideFlags;
}

void SystemMonitor::moveCenter()
{
    QPoint pos = QCursor::pos();
    QRect primaryGeometry;
    for (QScreen *screen : qApp->screens()) {
        if (screen->geometry().contains(pos)) {
            primaryGeometry = screen->geometry();
        }
    }

    if (primaryGeometry.isEmpty()) {
        primaryGeometry = qApp->primaryScreen()->geometry();
    }

    this->move(primaryGeometry.x() + (primaryGeometry.width() - this->width())/2,
               primaryGeometry.y() + (primaryGeometry.height() - this->height())/2);
}

void SystemMonitor::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void SystemMonitor::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    QPainterPath path;
    path.addRect(QRectF(rect()));
    painter.setOpacity(1);
    painter.fillPath(path, QColor("#FFFFFF"));
}

void SystemMonitor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        this->dragPosition = event->globalPos() - frameGeometry().topLeft();
        this->mousePressed = true;
    }
    QFrame::mousePressEvent(event);
}

void SystemMonitor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        this->mousePressed = false;
    }

    QFrame::mouseReleaseEvent(event);
}

void SystemMonitor::mouseMoveEvent(QMouseEvent *event)
{
    if (this->mousePressed) {
        move(event->globalPos() - this->dragPosition);
    }

    QFrame::mouseMoveEvent(event);
}
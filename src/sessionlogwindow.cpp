#include "sessionlogwindow.h"
#include "qsorecordformat.h"
#include "config.h"

#include <QListWidget>
#include <QVBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QFont>
#include <QFontMetrics>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QCloseEvent>

SessionLogWindow::SessionLogWindow(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowTitleHint | Qt::WindowMinimizeButtonHint
                    | Qt::WindowCloseButtonHint)
{
    setWindowTitle(tr("当次日志"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setUniformItemSizes(true);
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(10);
    m_list->setFont(mono);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        onItemDoubleClicked(m_list->row(item));
    });
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &SessionLogWindow::onContextMenu);

    loadGeometryFromConfig();
}

int SessionLogWindow::configuredHeight() const
{
    const int h = theConfig.getInt(QStringLiteral("SessionLog/Height"), 0);
    if (h > 40)
        return h;
    const int linePx = QFontMetrics(m_list->font()).height() + 4;
    return linePx * 5 + layout()->contentsMargins().top()
           + layout()->contentsMargins().bottom() + 8;
}

void SessionLogWindow::loadGeometryFromConfig()
{
    m_attached = theConfig.getBool(QStringLiteral("SessionLog/Attached"), true);
    if (m_attached) {
        resize(qMax(400, theConfig.getInt(QStringLiteral("SessionLog/Width"), 0)),
               configuredHeight());
    } else {
        const int w = theConfig.getInt(QStringLiteral("SessionLog/Width"), 800);
        const int h = configuredHeight();
        const int x = theConfig.getInt(QStringLiteral("SessionLog/X"), 100);
        const int y = theConfig.getInt(QStringLiteral("SessionLog/Y"), 100);
        setGeometry(x, y, qMax(300, w), h);
    }
}

void SessionLogWindow::saveGeometryToConfig() const
{
    theConfig.set(QStringLiteral("SessionLog/Attached"), m_attached);
    theConfig.set(QStringLiteral("SessionLog/X"), x());
    theConfig.set(QStringLiteral("SessionLog/Y"), y());
    theConfig.set(QStringLiteral("SessionLog/Width"), width());
    theConfig.set(QStringLiteral("SessionLog/Height"), height());
}

void SessionLogWindow::syncAttachedGeometry()
{
    if (!m_attached || !m_mainWindow)
        return;

    m_syncingGeometry = true;
    const QRect mg = m_mainWindow->frameGeometry();
    const int h = configuredHeight();
    setGeometry(mg.x(), mg.y() - h, mg.width(), h);
    m_syncingGeometry = false;
}

void SessionLogWindow::appendRecord(const adif::Record &rec)
{
    m_records.push_back(rec);
    m_list->addItem(formatSessionLogLine(rec));
    scrollToBottom();
}

void SessionLogWindow::updateRecordAt(int index, const adif::Record &rec)
{
    if (index < 0 || index >= static_cast<int>(m_records.size()))
        return;
    m_records[static_cast<size_t>(index)] = rec;
    QListWidgetItem *item = m_list->item(index);
    if (item)
        item->setText(formatSessionLogLine(rec));
}

void SessionLogWindow::removeRecordAt(int index)
{
    if (index < 0 || index >= static_cast<int>(m_records.size()))
        return;
    m_records.erase(m_records.begin() + index);
    delete m_list->takeItem(index);
}

int SessionLogWindow::recordCount() const
{
    return static_cast<int>(m_records.size());
}

adif::Record SessionLogWindow::recordAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_records.size()))
        return adif::Record{};
    return m_records[static_cast<size_t>(index)];
}

void SessionLogWindow::scrollToBottom()
{
    if (m_list->count() > 0)
        m_list->scrollToItem(m_list->item(m_list->count() - 1));
}

void SessionLogWindow::onItemDoubleClicked(int row)
{
    if (row >= 0 && row < recordCount())
        emit editRecordRequested(row);
}

void SessionLogWindow::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_list->itemAt(pos);
    if (!item)
        return;

    const int row = m_list->row(item);
    QMenu menu(this);
    QAction *delAct = menu.addAction(tr("删除记录…"));
    QAction *chosen = menu.exec(m_list->mapToGlobal(pos));
    if (chosen == delAct)
        emit deleteRecordRequested(row);
}

void SessionLogWindow::moveEvent(QMoveEvent *event)
{
    QWidget::moveEvent(event);
    if (m_syncingGeometry)
        return;
    if (m_attached)
        m_attached = false;
    saveGeometryToConfig();
}

void SessionLogWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_syncingGeometry)
        return;
    if (m_attached)
        m_attached = false;
    saveGeometryToConfig();
}

void SessionLogWindow::closeEvent(QCloseEvent *event)
{
    saveGeometryToConfig();
    event->ignore();
    hide();
}

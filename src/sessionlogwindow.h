#ifndef SESSIONLOGWINDOW_H
#define SESSIONLOGWINDOW_H

#include <QWidget>
#include <vector>
#include "adif/record.h"

class QListWidget;
class QMainWindow;

class SessionLogWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SessionLogWindow(QWidget *parent = nullptr);

    bool isAttached() const { return m_attached; }
    void setMainWindow(QMainWindow *main) { m_mainWindow = main; }

    void loadGeometryFromConfig();
    void saveGeometryToConfig() const;
    void syncAttachedGeometry();

    void appendRecord(const adif::Record &rec);
    void updateRecordAt(int index, const adif::Record &rec);
    void removeRecordAt(int index);
    int recordCount() const;
    adif::Record recordAt(int index) const;

signals:
    void deleteRecordRequested(int index);
    void editRecordRequested(int index);

protected:
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onItemDoubleClicked(int row);
    void onContextMenu(const QPoint &pos);

private:
    int configuredHeight() const;
    void scrollToBottom();
    void trySnapToMainWindow();

    QMainWindow *m_mainWindow = nullptr;
    QListWidget *m_list = nullptr;
    std::vector<adif::Record> m_records;
    bool m_attached = true;
    bool m_syncingGeometry = false;
};

#endif

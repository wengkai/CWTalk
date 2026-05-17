#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QTableWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onBrowseAdifPath();
    void onAccepted();

private:
    void loadFromConfig();
    void saveToConfig();
    void refreshSerialPortLists();
    void populateSerialPortCombo(QComboBox *combo, const QString &selectedPort);
    QString serialPortFromCombo(const QComboBox *combo) const;
    void updateCatControlsEnabled();

    QWidget *makeHardwareTab();
    QWidget *makeStationTab();
    QWidget *makeOperationTab();
    QWidget *makeFilesTab();
    QWidget *makeShortcutsTab();

    // Hardware
    QComboBox *m_catPort = nullptr;
    QComboBox *m_catBaud = nullptr;
    QSpinBox *m_catAddress = nullptr;
    QComboBox *m_keyingPort = nullptr;
    QComboBox *m_keyingLine = nullptr;
    QCheckBox *m_keyingActiveLow = nullptr;
    QCheckBox *m_catEnabled = nullptr;
    QComboBox *m_catBackend = nullptr;
    QSpinBox *m_catPollMs = nullptr;

    // Station
    QLineEdit *m_myCall = nullptr;
    QLineEdit *m_myName = nullptr;
    QLineEdit *m_myQth = nullptr;
    QLineEdit *m_myRig = nullptr;
    QLineEdit *m_grid = nullptr;
    QSpinBox *m_power = nullptr;

    // Operation
    QSpinBox *m_defaultWpm = nullptr;
    // QLineEdit *m_cqMessage = nullptr;
    QSpinBox *m_cqInterval = nullptr;
    QSpinBox *m_sendClearDelay = nullptr;

    // Files
    QLineEdit *m_adifPath = nullptr;

    // Shortcuts
    QTableWidget *m_shortcutTable = nullptr;
};

#endif

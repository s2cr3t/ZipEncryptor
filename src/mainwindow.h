#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QLabel>

class ZipEncryptor;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void browseDirectory();
    void refreshFileList();
    void selectAllFiles();
    void deselectAllFiles();
    void validateInput();
    void startEncryption();
    void cancelEncryption();
    void updateProgress(int fileIndex, int totalFiles, const QString& currentFile);
    void encryptionCompleted();
    void encryptionFailed(const QString& errorMessage);
    void showAboutDialog();
    void updatePasswordStrength();
    void updateFileCountLabel();
    void updateTime();

private:
    Ui::MainWindow *ui;
    ZipEncryptor* m_encryptor = nullptr;
    bool m_isEncrypting;
    
    // 状态栏标签
    QLabel* m_statusLabel;
    QLabel* m_versionLabel;
    QLabel* m_timeLabel;

    void initializeUI();
    void connectSignalsAndSlots();
    void applyBlurEffect();
    void applyModernStyle();
    void setupStatusBar();
    void resetUIAfterEncryption();
    void cleanupEncryptor();
    void loadSettings();
    void saveSettings();
    bool hasComplexPassword(const QString& password);
    QString formatFileSize(qint64 bytes);
};
#endif // MAINWINDOW_H

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsEffect>
#include <QGraphicsDropShadowEffect>
#include <QListWidgetItem>
#include <QThread>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QTimer>
#include <QDateTime>
#include <QSettings>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>
#include <QDebug>
#include <QRegExp>
#include "zipencryptor.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isEncrypting(false)
    , m_statusLabel(nullptr)
    , m_versionLabel(nullptr)
    , m_timeLabel(nullptr)
{
    ui->setupUi(this);
    
    // 设置窗口标题
    setWindowTitle(tr("ZIP文件加密工具 - 个人版"));
    
    // 初始化界面
    initializeUI();
    
    // 连接信号和槽
    connectSignalsAndSlots();
    
    // 加载上次使用的目录
    loadSettings();
    
    // 状态栏显示就绪信息
    ui->statusbar->showMessage(tr("就绪"));
}


MainWindow::~MainWindow()
{
    // 保存设置
    saveSettings();
    
    delete ui;
}

void MainWindow::initializeUI()
{
    // 设置按钮初始状态
    ui->encryptButton->setEnabled(false);
    ui->cancelButton->setEnabled(false);
    
    // 设置进度条初始状态
    ui->totalProgressBar->setValue(0);
    ui->statusLabel->setText(tr("就绪"));
    
    // 设置文件列表选择模式
    ui->fileListWidget->setSelectionMode(QAbstractItemView::MultiSelection);
    
    // 初始化文件计数标签
    updateFileCountLabel();
    
    // 设置密码显示/隐藏
    connect(ui->showPasswordCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        ui->passwordLineEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        ui->confirmPasswordLineEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    
    // 密码强度检测
    connect(ui->passwordLineEdit, &QLineEdit::textChanged, this, &MainWindow::updatePasswordStrength);
    
    // 设置状态栏
    setupStatusBar();
}

void MainWindow::updateFileCountLabel()
{
    int selectedCount = ui->fileListWidget->selectedItems().count();
    int totalCount = ui->fileListWidget->count();
    
    // 排除"未找到ZIP文件"的提示项
    if (totalCount > 0) {
        QListWidgetItem* firstItem = ui->fileListWidget->item(0);
        if (firstItem && !(firstItem->flags() & Qt::ItemIsSelectable)) {
            totalCount = 0;
            selectedCount = 0;
        }
    }
    
    ui->fileCountLabel->setText(tr("已选择: %1 / %2 个文件").arg(selectedCount).arg(totalCount));
}

void MainWindow::updatePasswordStrength()
{
    QString password = ui->passwordLineEdit->text();
    QString strength;
    QString color;
    
    if (password.isEmpty()) {
        strength = tr("无");
        color = "#6c757d";
    } else if (password.length() < 6) {
        strength = tr("弱");
        color = "#dc3545";
    } else if (password.length() < 8 || !hasComplexPassword(password)) {
        strength = tr("中等");
        color = "#ffc107";
    } else {
        strength = tr("强");
        color = "#28a745";
    }
    
    ui->passwordStrengthLabel->setText(tr("密码强度: %1").arg(strength));
    ui->passwordStrengthLabel->setStyleSheet(QString("QLabel { color: %1; font-weight: 600; }").arg(color));
}

bool MainWindow::hasComplexPassword(const QString& password)
{
    bool hasLower = password.contains(QRegExp("[a-z]"));
    bool hasUpper = password.contains(QRegExp("[A-Z]"));
    bool hasDigit = password.contains(QRegExp("[0-9]"));
    bool hasSpecial = password.contains(QRegExp("[!@#$%^&*()_+\\-=\\[\\]{};':\"\\\\|,.<>\\/?]"));
    
    return (hasLower + hasUpper + hasDigit + hasSpecial) >= 3;
}

void MainWindow::applyModernStyle()
{
    // 移除过度的样式设置，保持简洁
    // 不再设置无边框窗口，保持系统原生窗口控制
}

void MainWindow::setupStatusBar()
{
    // 创建状态栏标签
    m_statusLabel = new QLabel(tr("就绪"), this);
    m_versionLabel = new QLabel(tr("v1.0.0"), this);
    m_timeLabel = new QLabel(this);
    
    // 添加到状态栏
    ui->statusbar->addWidget(m_statusLabel, 1);
    ui->statusbar->addPermanentWidget(m_versionLabel);
    ui->statusbar->addPermanentWidget(m_timeLabel);
    
    // 启动时间更新定时器
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateTime);
    timer->start(1000);
    updateTime();
}

void MainWindow::updateTime()
{
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    m_timeLabel->setText(currentTime);
}

void MainWindow::connectSignalsAndSlots()
{
    // 浏览按钮
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::browseDirectory);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::browseDirectory);
    
    // 刷新列表按钮
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::refreshFileList);
    
    // 全选/取消全选按钮
    connect(ui->selectAllButton, &QPushButton::clicked, this, &MainWindow::selectAllFiles);
    connect(ui->deselectAllButton, &QPushButton::clicked, this, &MainWindow::deselectAllFiles);
    
    // 加密按钮
    connect(ui->encryptButton, &QPushButton::clicked, this, &MainWindow::startEncryption);
    
    // 取消按钮
    connect(ui->cancelButton, &QPushButton::clicked, this, &MainWindow::cancelEncryption);
    
    // 退出动作
    connect(ui->actionExit, &QAction::triggered, this, &QMainWindow::close);
    
    // 关于动作
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
    
    // 密码输入框变化
    connect(ui->passwordLineEdit, &QLineEdit::textChanged, this, &MainWindow::validateInput);
    connect(ui->confirmPasswordLineEdit, &QLineEdit::textChanged, this, &MainWindow::validateInput);
    
    // 文件列表选择变化
    connect(ui->fileListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::validateInput);
    
    // 目录输入框变化
    connect(ui->directoryLineEdit, &QLineEdit::textChanged, this, &MainWindow::validateInput);
}

void MainWindow::applyBlurEffect()
{
    // 创建毛玻璃效果
    QGraphicsBlurEffect* blurEffect = new QGraphicsBlurEffect(this);
    blurEffect->setBlurRadius(10);
    blurEffect->setBlurHints(QGraphicsBlurEffect::QualityHint);
    
    // 设置窗口背景透明度
    setWindowOpacity(0.98);
}

void MainWindow::browseDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择目录"),
                                                   ui->directoryLineEdit->text(),
                                                   QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!dir.isEmpty()) {
        ui->directoryLineEdit->setText(dir);
        refreshFileList();
    }
}

void MainWindow::refreshFileList()
{
    ui->fileListWidget->clear();
    
    QString dirPath = ui->directoryLineEdit->text();
    if (dirPath.isEmpty()) {
        return;
    }
    
    QDir dir(dirPath);
    QStringList filters;
    filters << "*.zip";
    dir.setNameFilters(filters);
    
    QFileInfoList fileList = dir.entryInfoList();
    
    if (fileList.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem(tr("📂 未找到ZIP文件"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        item->setForeground(QColor("#6c757d"));
        ui->fileListWidget->addItem(item);
        updateFileCountLabel();
        return;
    }
    
    for (const QFileInfo& fileInfo : fileList) {
        QListWidgetItem* item = new QListWidgetItem();
        
        // 格式化文件信息
        QString fileName = fileInfo.fileName();
        QString fileSize = formatFileSize(fileInfo.size());
        QString displayText = QString("📄 %1 (%2)").arg(fileName, fileSize);
        
        item->setText(displayText);
        item->setData(Qt::UserRole, fileInfo.absoluteFilePath());
        item->setToolTip(tr("文件: %1\n大小: %2\n路径: %3")
                        .arg(fileName, fileSize, fileInfo.absoluteFilePath()));
        
        ui->fileListWidget->addItem(item);
    }
    
    updateFileCountLabel();
    validateInput();
}

void MainWindow::selectAllFiles()
{
    for (int i = 0; i < ui->fileListWidget->count(); ++i) {
        QListWidgetItem* item = ui->fileListWidget->item(i);
        if (item->flags() & Qt::ItemIsSelectable) {
            item->setSelected(true);
        }
    }
}

QString MainWindow::formatFileSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    
    if (bytes >= GB) {
        return QString::number(bytes / (double)GB, 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / (double)MB, 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / (double)KB, 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

void MainWindow::deselectAllFiles()
{
    ui->fileListWidget->clearSelection();
}

void MainWindow::validateInput()
{
    bool isValid = true;
    QString errorMessage;
    
    // 检查目录
    if (ui->directoryLineEdit->text().isEmpty()) {
        isValid = false;
        errorMessage = tr("请选择目录");
    }
    
    // 检查文件选择
    else if (ui->fileListWidget->selectedItems().isEmpty()) {
        isValid = false;
        errorMessage = tr("请选择要加密的文件");
    }
    
    // 检查密码
    else if (ui->passwordLineEdit->text().isEmpty()) {
        isValid = false;
        errorMessage = tr("请输入密码");
    }
    
    // 检查密码确认
    else if (ui->passwordLineEdit->text() != ui->confirmPasswordLineEdit->text()) {
        isValid = false;
        errorMessage = tr("两次输入的密码不一致");
    }
    
    // 检查密码强度
    else if (ui->passwordLineEdit->text().length() < 6) {
        isValid = false;
        errorMessage = tr("密码长度至少6位");
    }
    
    ui->encryptButton->setEnabled(isValid && !m_isEncrypting);
    
    // 更新状态栏
    if (!isValid && !errorMessage.isEmpty()) {
        m_statusLabel->setText(errorMessage);
        m_statusLabel->setStyleSheet("QLabel { color: #dc3545; }");
    } else if (!m_isEncrypting) {
        m_statusLabel->setText(tr("就绪"));
        m_statusLabel->setStyleSheet("QLabel { color: #28a745; }");
    }
    
    // 更新文件计数
    updateFileCountLabel();
}

void MainWindow::startEncryption()
{
    // 检查密码
    QString password = ui->passwordLineEdit->text();
    QString confirmPassword = ui->confirmPasswordLineEdit->text();
    
    if (password != confirmPassword) {
        QMessageBox::warning(this, tr("密码错误"), tr("两次输入的密码不一致，请重新输入。"));
        return;
    }
    
    // 获取选中的文件
    QList<QListWidgetItem*> selectedItems = ui->fileListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::warning(this, tr("未选择文件"), tr("请选择至少一个ZIP文件进行加密。"));
        return;
    }
    
    // 准备加密任务
    QStringList filesToEncrypt;
    for (QListWidgetItem* item : selectedItems) {
        filesToEncrypt.append(item->data(Qt::UserRole).toString());
    }
    
    // 更新UI状态
    m_isEncrypting = true;
    ui->encryptButton->setEnabled(false);
    ui->cancelButton->setEnabled(true);
    ui->browseButton->setEnabled(false);
    ui->refreshButton->setEnabled(false);
    ui->selectAllButton->setEnabled(false);
    ui->deselectAllButton->setEnabled(false);
    ui->fileListWidget->setEnabled(false);
    ui->passwordLineEdit->setEnabled(false);
    ui->confirmPasswordLineEdit->setEnabled(false);
    ui->showPasswordCheckBox->setEnabled(false);
    
    // 重置进度条
    ui->totalProgressBar->setValue(0);
    ui->totalProgressBar->setMaximum(filesToEncrypt.size());
    ui->statusLabel->setText(tr("准备加密..."));
    
    // 创建加密器
    m_encryptor = new ZipEncryptor(this);
    
    // 连接信号
    connect(m_encryptor, &ZipEncryptor::progressUpdated, this, &MainWindow::updateProgress);
    connect(m_encryptor, &ZipEncryptor::encryptionCompleted, this, &MainWindow::encryptionCompleted);
    connect(m_encryptor, &ZipEncryptor::encryptionFailed, this, &MainWindow::encryptionFailed);
    
    // 开始加密
    m_encryptor->encryptFiles(filesToEncrypt, password);
}

void MainWindow::cancelEncryption()
{
    if (m_encryptor) {
        m_encryptor->cancel();
    }
}

void MainWindow::updateProgress(int fileIndex, int totalFiles, const QString& currentFile)
{
    // 更新进度条
    ui->totalProgressBar->setValue(fileIndex);
    
    // 更新状态标签
    double percentage = (fileIndex * 100.0) / totalFiles;
    ui->statusLabel->setText(tr("正在加密: %1 (%2/%3, %4%)").arg(QFileInfo(currentFile).fileName())
                                                       .arg(fileIndex)
                                                       .arg(totalFiles)
                                                       .arg(percentage, 0, 'f', 1));
    
    // 更新状态栏
    ui->statusbar->showMessage(tr("加密进行中..."));
}

void MainWindow::encryptionCompleted()
{
    // 更新UI状态
    resetUIAfterEncryption();
    
    // 更新进度条和状态
    ui->totalProgressBar->setValue(ui->totalProgressBar->maximum());
    ui->statusLabel->setText(tr("加密完成"));
    ui->statusbar->showMessage(tr("加密完成"), 5000);
    
    // 显示完成消息
    QMessageBox::information(this, tr("加密完成"), tr("所有选中的ZIP文件已成功加密。"));
    
    // 清理
    cleanupEncryptor();
}

void MainWindow::encryptionFailed(const QString& errorMessage)
{
    // 更新UI状态
    resetUIAfterEncryption();
    
    // 更新状态
    ui->statusLabel->setText(tr("加密失败"));
    ui->statusbar->showMessage(tr("加密失败"), 5000);
    
    // 显示错误消息
    QMessageBox::critical(this, tr("加密失败"), tr("加密过程中发生错误: %1").arg(errorMessage));
    
    // 清理
    cleanupEncryptor();
}

void MainWindow::resetUIAfterEncryption()
{
    m_isEncrypting = false;
    ui->browseButton->setEnabled(true);
    ui->refreshButton->setEnabled(true);
    ui->selectAllButton->setEnabled(true);
    ui->deselectAllButton->setEnabled(true);
    ui->fileListWidget->setEnabled(true);
    ui->passwordLineEdit->setEnabled(true);
    ui->confirmPasswordLineEdit->setEnabled(true);
    ui->showPasswordCheckBox->setEnabled(true);
    ui->cancelButton->setEnabled(false);
    
    validateInput();
}

void MainWindow::cleanupEncryptor()
{
    if (m_encryptor) {
        m_encryptor->deleteLater();
        m_encryptor = nullptr;
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, tr("关于 ZIP文件加密工具"),
                      tr("<h3>ZIP文件加密工具 - 个人版</h3>"
                         "<p>版本 1.0.0</p>"
                         "<p>一款专业的ZIP文件加密工具，支持批量加密操作。</p>"
                         "<p>纸飞机 © 2025 版权所有</p>"));
}

void MainWindow::loadSettings()
{
    QSettings settings("ZipEncryptor", "ZipEncryptorApp");
    QString lastDir = settings.value("lastDirectory", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    
    ui->directoryLineEdit->setText(lastDir);
    refreshFileList();
}

void MainWindow::saveSettings()
{
    QSettings settings("ZipEncryptor", "ZipEncryptorApp");
    settings.setValue("lastDirectory", ui->directoryLineEdit->text());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_isEncrypting) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, tr("确认退出"),
                                                                tr("加密操作正在进行中，确定要退出吗？"),
                                                                QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
        
        // 取消加密
        cancelEncryption();
    }
    
    // 保存设置
    saveSettings();
    
    event->accept();
}

#include "zipencryptor.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QCoreApplication>
#include <QDebug>
#include <QTemporaryDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QBuffer>
#include <QDataStream>
#include <QTime>

// ZIP文件格式常量
#define ZIP_LOCAL_HEADER_SIGNATURE      0x04034b50
#define ZIP_CENTRAL_DIR_SIGNATURE       0x02014b50
#define ZIP_END_OF_CENTRAL_DIR_SIGNATURE 0x06054b50
#define ZIP_DATA_DESCRIPTOR_SIGNATURE   0x08074b50

// ZIP加密常量
#define ZIP_VERSION_NEEDED_TO_EXTRACT   20  // 2.0版本
#define ZIP_GENERAL_PURPOSE_BIT_FLAG    0x0001  // 加密标志
#define ZIP_COMPRESSION_METHOD_STORED   0  // 存储（不压缩）
#define ZIP_COMPRESSION_METHOD_DEFLATED 8  // deflate压缩
#define ZIP_ENCRYPTION_HEADER_SIZE      12

ZipEncryptor::ZipEncryptor(QObject *parent)
    : QObject(parent)
    , m_isCancelled(false)
{
    // 初始化随机数种子
    qsrand(static_cast<uint>(QTime::currentTime().msec()));
}

ZipEncryptor::~ZipEncryptor()
{
}

void ZipEncryptor::encryptFiles(const QStringList& files, const QString& password)
{
    // 保存参数
    m_files = files;
    m_password = password;
    m_isCancelled = false;

    // 创建工作线程
    QThread* workerThread = new QThread();

    // 创建工作对象（没有父对象，可以移动到线程）
    ZipEncryptorWorker* worker = new ZipEncryptorWorker(files, password);
    worker->moveToThread(workerThread);

    // 连接信号
    connect(worker, &ZipEncryptorWorker::progressUpdated, this, &ZipEncryptor::progressUpdated);
    connect(worker, &ZipEncryptorWorker::encryptionCompleted, this, &ZipEncryptor::encryptionCompleted);
    connect(worker, &ZipEncryptorWorker::encryptionFailed, this, &ZipEncryptor::encryptionFailed);

    // 连接取消信号
    connect(this, &ZipEncryptor::cancelRequested, worker, &ZipEncryptorWorker::cancel);

    // 线程启动时开始工作
    connect(workerThread, &QThread::started, worker, &ZipEncryptorWorker::process);

    // 工作完成后清理
    connect(worker, &ZipEncryptorWorker::finished, workerThread, &QThread::quit);
    connect(worker, &ZipEncryptorWorker::finished, worker, &ZipEncryptorWorker::deleteLater);
    connect(workerThread, &QThread::finished, workerThread, &QThread::deleteLater);

    // 启动线程
    workerThread->start();
}

void ZipEncryptor::cancel()
{
    m_isCancelled = true;
    emit cancelRequested();
}

// 工作类实现
ZipEncryptorWorker::ZipEncryptorWorker(const QStringList& files, const QString& password, QObject* parent)
    : QObject(parent)
    , m_files(files)
    , m_password(password)
    , m_isCancelled(false)
{
}

void ZipEncryptorWorker::process()
{
    int totalFiles = m_files.size();
    int currentIndex = 0;
    bool success = true;
    QString errorMessage;

    for (const QString& file : m_files) {
        // 检查是否取消
        if (m_isCancelled) {
            success = false;
            errorMessage = tr("操作已取消");
            break;
        }

        // 更新进度
        emit progressUpdated(currentIndex, totalFiles, file);

        // 加密文件
        if (!encryptFile(file, m_password)) {
            success = false;
            errorMessage = tr("加密文件失败: %1").arg(file);
            break;
        }

        // 增加索引
        currentIndex++;

        // 处理事件
        QCoreApplication::processEvents();

        // 短暂延迟，避免UI冻结
        QThread::msleep(50);
    }

    // 发送完成或失败信号
    if (success) {
        emit encryptionCompleted();
    } else {
        emit encryptionFailed(errorMessage);
    }

    // 发送完成信号
    emit finished();
}

void ZipEncryptorWorker::cancel()
{
    m_isCancelled = true;
}

bool ZipEncryptorWorker::encryptFile(const QString& filePath, const QString& password)
{
    QMutexLocker locker(&m_mutex);

    // 运行已知值测试
    testWithKnownValues();

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "文件不存在: " << filePath;
        return false;
    }

    // 检查文件是否已加密
    if (isFileEncrypted(filePath)) {
        // 文件已加密，跳过
        qDebug() << "文件已加密，跳过: " << filePath;
        return true;
    }

    // 生成加密后的文件名
    QString encryptedFilePath = generateEncryptedFileName(filePath);
    qDebug() << "加密后文件路径: " << encryptedFilePath;

    try {
        // 读取源文件
        QFile sourceFile(filePath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            qDebug() << "无法打开源文件: " << filePath;
            return false;
        }

        // 读取源文件内容
        QByteArray sourceData = sourceFile.readAll();
        sourceFile.close();

        qDebug() << "源文件大小: " << sourceData.size() << " 字节";
        qDebug() << "源文件前16字节: " << sourceData.left(16).toHex();

        // 创建目标加密ZIP文件
        QFile destFile(encryptedFilePath);
        if (!destFile.open(QIODevice::WriteOnly)) {
            qDebug() << "无法创建目标文件: " << encryptedFilePath;
            return false;
        }

        // 获取文件信息
        QString fileName = fileInfo.fileName();
        QDateTime modDateTime = fileInfo.lastModified();
        
        // 计算原始数据的CRC32值
        quint32 crc32Value = calculateCRC32(sourceData);
        qDebug() << "源文件CRC32: " << QString::number(crc32Value, 16).toUpper();
        
        // 压缩数据处理 - 对小文件使用存储方式
        QByteArray compressedData = sourceData;
        quint16 compressionMethod = ZIP_COMPRESSION_METHOD_STORED;
        qDebug() << "使用存储方式（不压缩）";
        
        // 初始化加密密钥数组（三个32位整数）
        quint32 keys[3];
        initKeys(password, keys);
        
        // 生成加密头（12字节）- 这会修改密钥状态
        QByteArray encryptionHeader = generateEncryptionHeader(keys, crc32Value);
        qDebug() << "生成加密头: " << encryptionHeader.toHex();
        
        // 直接使用生成加密头后的密钥状态来加密数据
        qDebug() << "数据加密时的密钥: " << QString::number(keys[0], 16).toUpper()
                 << QString::number(keys[1], 16).toUpper() 
                 << QString::number(keys[2], 16).toUpper();
        
        // 加密压缩数据
        QByteArray encryptedData = encryptData(compressedData, keys);
        qDebug() << "加密数据大小: " << encryptedData.size() << " 字节";
        qDebug() << "加密数据前16字节: " << encryptedData.left(16).toHex();
        
        // 计算最终压缩大小（加密头+加密数据）
        quint32 finalCompressedSize = encryptionHeader.size() + encryptedData.size();
        
        qDebug() << "写入本地文件头，压缩大小: " << finalCompressedSize 
                 << ", 原始大小: " << sourceData.size();
        
        // 写入本地文件头
        quint32 localHeaderOffset = destFile.pos();
        writeLocalFileHeader(destFile, fileName, modDateTime, crc32Value,
                            finalCompressedSize, sourceData.size(), compressionMethod);
        
        // 写入加密头
        destFile.write(encryptionHeader);
        
        // 写入加密数据
        destFile.write(encryptedData);
        
        // 保存中央目录的偏移量
        quint32 centralDirOffset = destFile.pos();
        
        // 写入中央目录
        writeCentralDirectory(destFile, fileName, modDateTime, crc32Value,
                             finalCompressedSize, sourceData.size(), 
                             localHeaderOffset, compressionMethod);
        
        // 写入中央目录结束记录
        writeEndOfCentralDirectory(destFile, 1, centralDirOffset,
                                  destFile.pos() - centralDirOffset);
        
        destFile.close();
        qDebug() << "文件加密成功: " << filePath << " -> " << encryptedFilePath;
        
        // 验证生成的文件
        QFile verifyFile(encryptedFilePath);
        if (verifyFile.open(QIODevice::ReadOnly)) {
            QByteArray fileHeader = verifyFile.read(50);
            qDebug() << "生成文件的前50字节: " << fileHeader.toHex();
            verifyFile.close();
        }
        
        return true;
    } catch (const std::exception& e) {
        qDebug() << "加密过程中发生异常: " << e.what();
        return false;
    } catch (...) {
        qDebug() << "加密过程中发生未知异常";
        return false;
    }
}

void ZipEncryptorWorker::writeLocalFileHeader(QFile& file, const QString& fileName,
                                             const QDateTime& modDateTime, quint32 crc32,
                                             quint32 compressedSize, quint32 uncompressedSize,
                                             quint16 compressionMethod)
{
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    
    // 本地文件头签名
    out << (quint32)ZIP_LOCAL_HEADER_SIGNATURE;
    
    // 解压所需版本（2.0用于加密文件）
    out << (quint16)ZIP_VERSION_NEEDED_TO_EXTRACT;
    
    // 检查文件名是否包含非ASCII字符
    QByteArray fileNameBytes = fileName.toUtf8();
    bool hasNonAscii = false;
    for (int i = 0; i < fileName.length(); ++i) {
        if (fileName.at(i).unicode() > 127) {
            hasNonAscii = true;
            break;
        }
    }
    
    // 通用位标志 - 关键修复：为中文文件名设置UTF-8标志
    quint16 generalPurposeFlag = 0x0001; // 加密标志
    if (hasNonAscii) {
        generalPurposeFlag |= 0x0800; // UTF-8标志（第11位）
        qDebug() << "检测到非ASCII字符，设置UTF-8标志";
    }
    out << generalPurposeFlag;
    
    // 压缩方法
    out << compressionMethod;
    
    // 最后修改时间和日期
    out << (quint16)dosTime(modDateTime);
    out << (quint16)dosDate(modDateTime);
    
    // CRC-32校验 - 使用原始数据的CRC32值
    out << crc32;
    
    // 压缩后大小（包含加密头）
    out << compressedSize;
    
    // 未压缩大小
    out << uncompressedSize;
    
    // 文件名长度
    out << (quint16)fileNameBytes.size();
    
    // 扩展字段长度
    out << (quint16)0;
    
    // 文件名 - 使用UTF-8编码
    file.write(fileNameBytes);
    
    qDebug() << "本地文件头已写入: " 
             << "签名=" << QString::number(ZIP_LOCAL_HEADER_SIGNATURE, 16).toUpper()
             << "标志=" << QString::number(generalPurposeFlag, 16).toUpper()
             << "压缩方法=" << compressionMethod
             << "CRC32=" << QString::number(crc32, 16).toUpper()
             << "压缩大小=" << compressedSize
             << "原始大小=" << uncompressedSize
             << "文件名UTF-8字节数=" << fileNameBytes.size()
             << "原始文件名=" << fileName;
}


void ZipEncryptorWorker::writeCentralDirectory(QFile& file, const QString& fileName,
                                              const QDateTime& modDateTime, quint32 crc32,
                                              quint32 compressedSize, quint32 uncompressedSize,
                                              quint32 localHeaderOffset, quint16 compressionMethod)
{
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    
    // 中央目录文件头签名
    out << (quint32)ZIP_CENTRAL_DIR_SIGNATURE;
    
    // 创建版本 (使用2.0以保持兼容性)
    out << (quint16)20;
    
    // 解压所需版本（2.0）
    out << (quint16)ZIP_VERSION_NEEDED_TO_EXTRACT;
    
    // 检查文件名是否包含非ASCII字符
    QByteArray fileNameBytes = fileName.toUtf8();
    bool hasNonAscii = false;
    for (int i = 0; i < fileName.length(); ++i) {
        if (fileName.at(i).unicode() > 127) {
            hasNonAscii = true;
            break;
        }
    }
    
    // 通用位标志 - 与本地文件头保持一致
    quint16 generalPurposeFlag = 0x0001; // 加密标志
    if (hasNonAscii) {
        generalPurposeFlag |= 0x0800; // UTF-8标志（第11位）
    }
    out << generalPurposeFlag;
    
    // 压缩方法
    out << compressionMethod;
    
    // 最后修改时间和日期
    out << (quint16)dosTime(modDateTime);
    out << (quint16)dosDate(modDateTime);
    
    // CRC-32校验 - 使用原始数据的CRC32值
    out << crc32;
    
    // 压缩后大小（包含加密头）
    out << compressedSize;
    
    // 未压缩大小
    out << uncompressedSize;
    
    // 文件名长度
    out << (quint16)fileNameBytes.size();
    
    // 扩展字段长度
    out << (quint16)0;
    
    // 文件注释长度
    out << (quint16)0;
    
    // 磁盘开始号
    out << (quint16)0;
    
    // 内部文件属性
    out << (quint16)0;
    
    // 外部文件属性 (普通文件)
    out << (quint32)0x20; // 归档属性
    
    // 本地文件头的相对偏移量
    out << localHeaderOffset;
    
    // 文件名 - 使用UTF-8编码
    file.write(fileNameBytes);
    
    qDebug() << "中央目录已写入: "
             << "签名=" << QString::number(ZIP_CENTRAL_DIR_SIGNATURE, 16).toUpper()
             << "本地头偏移=" << localHeaderOffset
             << "UTF-8标志=" << (hasNonAscii ? "是" : "否")
             << "文件名=" << fileName;
}

// 添加一个创建标准测试ZIP文件的函数
bool ZipEncryptorWorker::createTestZipFile()
{
    qDebug() << "=== 创建标准测试ZIP文件 ===";
    
    QString testFilePath = "test_encrypted.zip";
    QFile testFile(testFilePath);
    if (!testFile.open(QIODevice::WriteOnly)) {
        qDebug() << "无法创建测试文件";
        return false;
    }
    
    // 测试数据
    QByteArray testData = "Hello, World!";
    QString fileName = "test.txt";
    QString password = "123";
    
    // 计算CRC32
    quint32 crc32Value = calculateCRC32(testData);
    qDebug() << "测试数据CRC32: " << QString::number(crc32Value, 16).toUpper();
    
    // 初始化密钥
    quint32 keys[3];
    initKeys(password, keys);
    
    // 生成加密头
    QByteArray encryptionHeader = generateEncryptionHeader(keys, crc32Value);
    
    // 加密数据
    QByteArray encryptedData = encryptData(testData, keys);
    
    // 计算压缩大小
    quint32 compressedSize = encryptionHeader.size() + encryptedData.size();
    
    // 写入本地文件头
    QDateTime now = QDateTime::currentDateTime();
    writeLocalFileHeader(testFile, fileName, now, crc32Value,
                        compressedSize, testData.size(), ZIP_COMPRESSION_METHOD_STORED);
    
    // 写入加密头和数据
    testFile.write(encryptionHeader);
    testFile.write(encryptedData);
    
    // 写入中央目录
    quint32 centralDirOffset = testFile.pos();
    writeCentralDirectory(testFile, fileName, now, crc32Value,
                         compressedSize, testData.size(), 0, ZIP_COMPRESSION_METHOD_STORED);
    
    // 写入中央目录结束记录
    writeEndOfCentralDirectory(testFile, 1, centralDirOffset,
                              testFile.pos() - centralDirOffset);
    
    testFile.close();
    qDebug() << "测试ZIP文件已创建: " << testFilePath;
    
    return true;
}

// 添加一个测试中文文件名的函数
bool ZipEncryptorWorker::testChineseFileName()
{
    qDebug() << "=== 测试中文文件名 ===";
    
    QString testFilePath = "中文测试_encrypted.zip";
    QFile testFile(testFilePath);
    if (!testFile.open(QIODevice::WriteOnly)) {
        qDebug() << "无法创建测试文件";
        return false;
    }
    
    // 测试数据
    QByteArray testData = "这是中文测试内容";
    QString fileName = "中文文件名测试.txt";
    QString password = "123";
    
    qDebug() << "测试文件名: " << fileName;
    qDebug() << "文件名UTF-8字节: " << fileName.toUtf8().toHex();
    
    // 计算CRC32
    quint32 crc32Value = calculateCRC32(testData);
    qDebug() << "测试数据CRC32: " << QString::number(crc32Value, 16).toUpper();
    
    // 初始化密钥
    quint32 keys[3];
    initKeys(password, keys);
    
    // 生成加密头
    QByteArray encryptionHeader = generateEncryptionHeader(keys, crc32Value);
    
    // 加密数据
    QByteArray encryptedData = encryptData(testData, keys);
    
    // 计算压缩大小
    quint32 compressedSize = encryptionHeader.size() + encryptedData.size();
    
    // 写入本地文件头
    QDateTime now = QDateTime::currentDateTime();
    writeLocalFileHeader(testFile, fileName, now, crc32Value,
                        compressedSize, testData.size(), ZIP_COMPRESSION_METHOD_STORED);
    
    // 写入加密头和数据
    testFile.write(encryptionHeader);
    testFile.write(encryptedData);
    
    // 写入中央目录
    quint32 centralDirOffset = testFile.pos();
    writeCentralDirectory(testFile, fileName, now, crc32Value,
                         compressedSize, testData.size(), 0, ZIP_COMPRESSION_METHOD_STORED);
    
    // 写入中央目录结束记录
    writeEndOfCentralDirectory(testFile, 1, centralDirOffset,
                              testFile.pos() - centralDirOffset);
    
    testFile.close();
    qDebug() << "中文测试ZIP文件已创建: " << testFilePath;
    
    return true;
}

void ZipEncryptorWorker::writeEndOfCentralDirectory(QFile& file, quint16 numEntries,
                                                  quint32 centralDirOffset,
                                                  quint32 centralDirSize)
{
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // 中央目录结束记录签名
    out << (quint32)ZIP_END_OF_CENTRAL_DIR_SIGNATURE;

    // 当前磁盘编号
    out << (quint16)0;

    // 中央目录开始磁盘编号
    out << (quint16)0;

    // 该磁盘上中央目录记录数
    out << numEntries;

    // 中央目录记录总数
    out << numEntries;

    // 中央目录大小
    out << centralDirSize;

    // 中央目录开始位置相对于archive开始的偏移
    out << centralDirOffset;

    // 注释长度
    out << (quint16)0;
}

quint16 ZipEncryptorWorker::dosTime(const QDateTime& dateTime)
{
    QTime time = dateTime.time();
    return (time.hour() << 11) | (time.minute() << 5) | (time.second() / 2);
}

quint16 ZipEncryptorWorker::dosDate(const QDateTime& dateTime)
{
    QDate date = dateTime.date();
    return ((date.year() - 1980) << 9) | (date.month() << 5) | date.day();
}

void ZipEncryptorWorker::initKeys(const QString& password, quint32 keys[3])
{
    // 添加密码调试信息
    qDebug() << "=== 密钥初始化 ===";
    qDebug() << "接收到的密码: '" << password << "'";
    qDebug() << "密码长度: " << password.length();
    qDebug() << "密码UTF-8字节: " << password.toUtf8().toHex();
    
    // 标准PKZIP加密的正确初始密钥值
    keys[0] = 0x12345678;
    keys[1] = 0x23456789;
    keys[2] = 0x34567890;
    
    qDebug() << "初始密钥值: " << QString::number(keys[0], 16).toUpper()
             << QString::number(keys[1], 16).toUpper() 
             << QString::number(keys[2], 16).toUpper();
    
    // 使用密码更新密钥
    QByteArray passBytes = password.toUtf8();
    
    // 使用密码中的每个字节更新密钥
    for (int i = 0; i < passBytes.size(); ++i) {
        quint8 passByte = static_cast<quint8>(passBytes[i]);
        qDebug() << "处理密码字节" << i << ": " << QString::number(passByte, 16).toUpper() 
                 << "(" << static_cast<char>(passByte) << ")";
        updateKeys(keys, passByte);
        qDebug() << "  更新后密钥: " << QString::number(keys[0], 16).toUpper()
                 << QString::number(keys[1], 16).toUpper() 
                 << QString::number(keys[2], 16).toUpper();
    }
    
    qDebug() << "最终密钥: " << QString::number(keys[0], 16).toUpper()
             << QString::number(keys[1], 16).toUpper() 
             << QString::number(keys[2], 16).toUpper();
}

// 让我们创建一个简单的测试来验证我们的加密是否与标准兼容
bool ZipEncryptorWorker::testWithKnownValues()
{
    qDebug() << "=== 已知值测试 ===";
    
    // 测试1: 空密码
    quint32 keys1[3];
    initKeys("", keys1);
    
    // 测试2: 简单密码 "a"
    quint32 keys2[3];
    initKeys("a", keys2);
    
    // 测试3: 您的密码 "123"
    quint32 keys3[3];
    initKeys("123", keys3);
    
    // 测试CRC32计算
    QByteArray testData = "Hello";
    quint32 testCRC = calculateCRC32(testData);
    qDebug() << "测试数据 'Hello' 的CRC32: " << QString::number(testCRC, 16).toUpper();
    
    // 已知的CRC32值验证
    // "Hello" 的标准CRC32应该是 0xF7D18982
    if (testCRC == 0xF7D18982) {
        qDebug() << "CRC32计算正确!";
    } else {
        qDebug() << "CRC32计算错误! 期望: F7D18982, 实际: " << QString::number(testCRC, 16).toUpper();
    }
    
    return true;
}

void ZipEncryptorWorker::updateKeys(quint32 keys[3], quint8 byteValue)
{
    // 更新密钥0：与字节进行CRC32计算
    keys[0] = crc32(keys[0], byteValue);
    
    // 更新密钥1：加上密钥0的低字节，乘以一个质数并加1
    keys[1] = keys[1] + (keys[0] & 0xFF);
    keys[1] = keys[1] * 134775813 + 1;
    
    // 更新密钥2：与密钥1的高字节进行CRC32计算
    keys[2] = crc32(keys[2], static_cast<quint8>((keys[1] >> 24) & 0xFF));
}

quint8 ZipEncryptorWorker::decryptByte(quint32 keys[3])
{
    // 标准PKZIP加密算法的临时值计算
    quint16 temp = static_cast<quint16>(keys[2] & 0xFFFF) | 2;
    
    // 返回混淆字节
    return static_cast<quint8>(((temp * (temp ^ 1)) >> 8) & 0xFF);
}

QByteArray ZipEncryptorWorker::generateEncryptionHeader(quint32 keys[3], quint32 crc32Value)
{
    QByteArray header;
    header.resize(ZIP_ENCRYPTION_HEADER_SIZE);
    
    // 保存初始密钥以便调试
    quint32 initialKeys[3] = {keys[0], keys[1], keys[2]};
    qDebug() << "生成加密头时的密钥: " << QString::number(initialKeys[0], 16).toUpper()
             << QString::number(initialKeys[1], 16).toUpper() 
             << QString::number(initialKeys[2], 16).toUpper();
    
    // 前11个字节使用随机值
    for (int i = 0; i < ZIP_ENCRYPTION_HEADER_SIZE - 1; ++i) {
        // 使用Qt 5.9.9兼容的qrand生成随机数
        quint8 randByte = static_cast<quint8>(qrand() % 256);
        quint8 tempByte = decryptByte(keys);
        header[i] = static_cast<char>(randByte ^ tempByte);
        updateKeys(keys, randByte);
    }
    
    // 关键修复：最后一个字节（索引11）必须是CRC32的最高字节
    // 这是PKZIP标准的密码验证机制
    quint8 lastByte = static_cast<quint8>((crc32Value >> 24) & 0xFF);
    quint8 tempByte = decryptByte(keys);
    header[11] = static_cast<char>(lastByte ^ tempByte);
    updateKeys(keys, lastByte);
    
    qDebug() << "加密头生成完成: " << header.toHex() 
             << "CRC32最高字节: " << QString::number(lastByte, 16).toUpper();
    
    qDebug() << "加密头生成后的密钥: " << QString::number(keys[0], 16).toUpper()
             << QString::number(keys[1], 16).toUpper() 
             << QString::number(keys[2], 16).toUpper();
    
    return header;
}

QByteArray ZipEncryptorWorker::encryptData(const QByteArray& data, quint32 keys[3])
{
    QByteArray encryptedData;
    encryptedData.resize(data.size());
    
    for (int i = 0; i < data.size(); ++i) {
        quint8 byteToEncrypt = static_cast<quint8>(data[i]);
        quint8 tempByte = decryptByte(keys);
        encryptedData[i] = static_cast<char>(byteToEncrypt ^ tempByte);
        updateKeys(keys, byteToEncrypt);
    }
    
    return encryptedData;
}

quint32 ZipEncryptorWorker::calculateCRC32(const QByteArray& data)
{
    quint32 crc = 0xFFFFFFFF;

    for (int i = 0; i < data.size(); ++i) {
        crc = crc32(crc, static_cast<quint8>(data[i]));
    }

    return ~crc;
}

// 添加一个测试函数来验证加密算法
bool ZipEncryptorWorker::testEncryptionAlgorithm()
{
    // 使用已知的测试数据验证算法
    QString testPassword = "test";
    QByteArray testData = "Hello World!";
    quint32 testCRC = calculateCRC32(testData);
    
    qDebug() << "=== 加密算法测试 ===";
    qDebug() << "测试密码: " << testPassword;
    qDebug() << "测试数据: " << testData;
    qDebug() << "测试CRC32: " << QString::number(testCRC, 16).toUpper();
    
    // 初始化密钥
    quint32 keys[3];
    initKeys(testPassword, keys);
    
    // 生成加密头
    QByteArray header = generateEncryptionHeader(keys, testCRC);
    qDebug() << "测试加密头: " << header.toHex();
    
    // 加密数据
    QByteArray encrypted = encryptData(testData, keys);
    qDebug() << "测试加密数据: " << encrypted.toHex();
    
    return true;
}

quint32 ZipEncryptorWorker::crc32(quint32 crc, quint8 byte)
{
    static const quint32 crcTable[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
        0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
        0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
        0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
        0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
        0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
        0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
        0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
        0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
        0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
        0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
        0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
        0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
                0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
        0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
        0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
        0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
        0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
        0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
        0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
        0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
        0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
        0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
        0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
        0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
        0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
        0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
        0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };

    return ((crc >> 8) & 0x00FFFFFF) ^ crcTable[(crc ^ byte) & 0xFF];
}

bool ZipEncryptorWorker::isFileEncrypted(const QString& filePath)
{
    // 检查文件名是否已包含加密标记
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();

    // 假设加密文件名包含"_encrypted"标记
    return fileName.contains("_encrypted", Qt::CaseInsensitive);
}

QString ZipEncryptorWorker::generateEncryptedFileName(const QString& originalFilePath)
{
    QFileInfo fileInfo(originalFilePath);
    QString baseName = fileInfo.baseName();
    QString path = fileInfo.path();
    
    // 生成时间戳
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    
    // 生成加密后的文件名（使用.zip后缀）
    QString encryptedFileName = QString("%1/%2_encrypted_%3.zip")
                               .arg(path)
                               .arg(baseName)
                               .arg(timestamp);
    
    return encryptedFileName;
}
#ifndef ZIPENCRYPTOR_H
#define ZIPENCRYPTOR_H

#include <QObject>
#include <QStringList>
#include <QMutex>
#include <QAtomicInt>
#include <QFile>
#include <QDateTime>

// 前向声明工作类
class ZipEncryptorWorker;

class ZipEncryptor : public QObject
{
    Q_OBJECT

public:
    explicit ZipEncryptor(QObject *parent = nullptr);
    ~ZipEncryptor();

    // 开始加密文件列表
    void encryptFiles(const QStringList& files, const QString& password);
    
    // 取消加密操作
    void cancel();

signals:
    // 进度更新信号
    void progressUpdated(int fileIndex, int totalFiles, const QString& currentFile);
    
    // 加密完成信号
    void encryptionCompleted();
    
    // 加密失败信号
    void encryptionFailed(const QString& errorMessage);
    
    // 内部使用：取消请求信号
    void cancelRequested();

private:
    QStringList m_files;
    QString m_password;
    QAtomicInt m_isCancelled;
};

// 工作类，用于在线程中执行实际加密操作
class ZipEncryptorWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit ZipEncryptorWorker(const QStringList& files, const QString& password, QObject* parent = nullptr);
    
public slots:
    // 处理加密任务
    void process();

    // 取消操作
    void cancel();
    
signals:
    // 进度更新信号
    void progressUpdated(int fileIndex, int totalFiles, const QString& currentFile);
    
    // 加密完成信号
    void encryptionCompleted();
    
    // 加密失败信号
    void encryptionFailed(const QString& errorMessage);
    
    // 完成信号（用于清理）
    void finished();
    
private:
    QStringList m_files;
    QString m_password;
    QAtomicInt m_isCancelled;
    QMutex m_mutex;
    
    // 加密单个ZIP文件
    bool encryptFile(const QString& filePath, const QString& password);
    
    // 检查文件是否已加密
    bool isFileEncrypted(const QString& filePath);
    
    // 生成加密后的文件名
    QString generateEncryptedFileName(const QString& originalFilePath);
    
    // 测试加密算法
    bool testEncryptionAlgorithm();

    bool testWithKnownValues();

    bool createTestZipFile();
    bool testChineseFileName();

    // 写入本地文件头
    void writeLocalFileHeader(QFile& file, const QString& fileName, 
                             const QDateTime& modDateTime, quint32 crc32,
                             quint32 compressedSize, quint32 uncompressedSize,
                             quint16 compressionMethod);
    
    // 写入中央目录
    void writeCentralDirectory(QFile& file, const QString& fileName, 
                              const QDateTime& modDateTime, quint32 crc32,
                              quint32 compressedSize, quint32 uncompressedSize,
                              quint32 localHeaderOffset, quint16 compressionMethod);
    
    // 写入中央目录结束记录
    void writeEndOfCentralDirectory(QFile& file, quint16 numEntries, 
                                   quint32 centralDirOffset, 
                                   quint32 centralDirSize);
    
    // 转换为DOS时间格式
    quint16 dosTime(const QDateTime& dateTime);
    
    // 转换为DOS日期格式
    quint16 dosDate(const QDateTime& dateTime);
    
    // 初始化加密密钥
    void initKeys(const QString& password, quint32 keys[3]);
    
    // 更新加密密钥
    void updateKeys(quint32 keys[3], quint8 byteValue);
    
    // 解密单个字节（用于生成加密字节）
    quint8 decryptByte(quint32 keys[3]);
    
    // 生成加密头
    QByteArray generateEncryptionHeader(quint32 keys[3], quint32 crc32Value);
    
    // 加密数据
    QByteArray encryptData(const QByteArray& data, quint32 keys[3]);
    
    // 计算CRC32校验和
    quint32 calculateCRC32(const QByteArray& data);
    
    // CRC32表查询
    quint32 crc32(quint32 crc, quint8 byte);
};

#endif // ZIPENCRYPTOR_H
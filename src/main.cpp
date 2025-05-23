#include <QApplication>
#include <QLibraryInfo>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    QApplication::setApplicationName("ZIP文件加密工具");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("ZipEncryptor");
    QApplication::setOrganizationDomain("zipencryptor.com");
    
    // 设置样式表
    QApplication::setStyle("Fusion");
    
    // 创建并显示主窗口
    MainWindow w;
    w.show();
    
    return a.exec();
}

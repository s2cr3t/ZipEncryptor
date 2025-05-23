QT += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# 强制静态链接配置
CONFIG += static staticlib
CONFIG -= shared

# 静态链接所有库
QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++

# 如果有静态Qt库，强制使用
win32 {
    QMAKE_LFLAGS += -Wl,-Bstatic
    LIBS += -Wl,-Bstatic
}

# 设置应用程序版本信息
VERSION = 1.0.0
QMAKE_TARGET_COMPANY = "ZipEncryptor"
QMAKE_TARGET_PRODUCT = "ZIP文件加密工具"
QMAKE_TARGET_DESCRIPTION = "企业级ZIP文件加密工具"
QMAKE_TARGET_COPYRIGHT = "© 2025 版权所有"

# 应用程序名称
TARGET = ZipEncryptor

# 输出目录
DESTDIR = $$PWD/bin

# 源文件
SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/zipencryptor.cpp

# 头文件
HEADERS += \
    src/mainwindow.h \
    src/zipencryptor.h

# UI文件
FORMS += \
    src/MainWindow.ui

# 中间文件目录
UI_DIR = $$OUT_PWD/ui
MOC_DIR = $$OUT_PWD/moc
OBJECTS_DIR = $$OUT_PWD/obj
RCC_DIR = $$OUT_PWD/rcc

CONFIG += zlib

# Windows特定配置
win32 {
    # 静态链接Windows库
    LIBS += -luser32 -lgdi32 -lshell32 -lole32 -loleaut32 -luuid -lwinmm -lwinspool -lcomdlg32 -ladvapi32
    
    target.path = $$PWD/deploy
    INSTALLS += target
}

# 默认规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
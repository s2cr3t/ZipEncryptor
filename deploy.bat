@echo off
echo ========================================
echo 创建自解压安装包
echo ========================================

set PROJECT_DIR=%~dp0
set TEMP_DIR=%PROJECT_DIR%temp_package
set QT_DIR=D:\qtcreator\5.9.9\mingw53_32

:: 清理并创建临时目录
if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"
mkdir "%TEMP_DIR%"

:: 复制所有必需文件
echo 复制程序文件...
copy /Y "%PROJECT_DIR%bin\ZipEncryptor.exe" "%TEMP_DIR%\"
copy /Y "%QT_DIR%\bin\Qt5Core.dll" "%TEMP_DIR%\"
copy /Y "%QT_DIR%\bin\Qt5Gui.dll" "%TEMP_DIR%\"
copy /Y "%QT_DIR%\bin\Qt5Widgets.dll" "%TEMP_DIR%\"
copy /Y "D:\qtcreator\Tools\mingw530_32\bin\libgcc_s_dw2-1.dll" "%TEMP_DIR%\"
copy /Y "D:\qtcreator\Tools\mingw530_32\bin\libstdc++-6.dll" "%TEMP_DIR%\"
copy /Y "D:\qtcreator\Tools\mingw530_32\bin\libwinpthread-1.dll" "%TEMP_DIR%\"

mkdir "%TEMP_DIR%\platforms"
copy /Y "%QT_DIR%\plugins\platforms\qwindows.dll" "%TEMP_DIR%\platforms\"

:: 创建批处理启动器
echo @echo off > "%TEMP_DIR%\ZipEncryptor.bat"
echo cd /d "%%~dp0" >> "%TEMP_DIR%\ZipEncryptor.bat"
echo ZipEncryptor.exe >> "%TEMP_DIR%\ZipEncryptor.bat"

:: 使用7zip创建自解压包（如果有的话）
if exist "C:\Program Files\7-Zip-Zstandard\7z.exe" (
    echo 创建自解压包...
    "C:\Program Files\7-Zip-Zstandard\7z.exe" a -sfx7z.sfx "%PROJECT_DIR%ZipEncryptor_Setup.exe" "%TEMP_DIR%\*"
    echo 自解压包创建完成: ZipEncryptor_Setup.exe
) else (
    echo 7-Zip未找到，创建ZIP包...
    powershell Compress-Archive -Path "%TEMP_DIR%\*" -DestinationPath "%PROJECT_DIR%ZipEncryptor_Portable.zip" -Force
    echo ZIP包创建完成: ZipEncryptor_Portable.zip
)

:: 清理临时目录
rmdir /s /q "%TEMP_DIR%"

echo ========================================
echo 打包完成！用户只需要解压即可使用
echo ========================================
pause
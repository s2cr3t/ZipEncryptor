# 初始化git仓库
git init

# 创建.gitignore文件（针对Qt项目）
echo "# Qt项目忽略文件
*.pro.user
*.pro.user.*
*.tmp
*.o
*.obj
*.so
*.dll
*.dylib
*.exe
build-*
debug/
release/
.qmake.stash
Makefile*
*.Debug
*.Release
moc_*.cpp
moc_*.h
qrc_*.cpp
ui_*.h
*.autosave
.DS_Store
Thumbs.db" > .gitignore

# 添加所有文件到暂存区
git add .

# 提交初始版本
git commit -m "Initial commit: ZipEncryptor project"
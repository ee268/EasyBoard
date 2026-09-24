# Windows 发布包

使用 MSVC2019 64 位和 `C:\Qt\5.15.2\msvc2019_64` 构建 Release。Qt WebEngine 和 PDF 渲染辅助程序都需要随主程序一起发布。

在 Visual Studio 2019 x64 开发者命令行中执行：

```powershell
cmake -S C:\qt_project_other\project\EasyBoard -B C:\qt_project_other\project\EasyBoard\build\release-msvc2019-x64 -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64
cmake --build C:\qt_project_other\project\EasyBoard\build\release-msvc2019-x64 --config Release --parallel 4
powershell -ExecutionPolicy Bypass -File C:\qt_project_other\project\EasyBoard\scripts\package-windows.ps1 -BuildDir C:\qt_project_other\project\EasyBoard\build\release-msvc2019-x64 -OutputDir C:\qt_project_other\project\EasyBoard\build\packages
```

脚本拒绝覆盖已有发布包。打包后，在不依赖本机 Qt 环境变量的 Windows 系统中解压并检查白板、网页视图、PDF 导入以及 `EasyBoardPdfRenderer.exe`。确认压缩包内存在 Qt WebEngine 运行文件，并核对 `SHA256SUMS.txt`。

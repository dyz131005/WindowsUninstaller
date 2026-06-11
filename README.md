# Windows 卸载程序测试工具

> ⚠️ **警告：此程序极其危险！**
> 
> 此程序用于虚拟机测试，会擦除系统磁盘并删除文件。
> **切勿在物理机上运行！**
> 
> 使用前请确保已创建虚拟机快照/系统还原点！

---

## 功能特性

- ✅ 使用 NSudo 提权到 TrustedInstaller 权限
- ✅ Windows 测试模式自动检测与启用
- ✅ 物理磁盘扇区擦除（写零）
- ✅ EFI 分区内容删除
- ✅ C 盘根目录内容删除
- ✅ 纯 GUI 界面（无控制台窗口）
- ✅ 完全静态链接（单文件部署）

## 技术实现

### 权限提升
- 使用 NSudo Launcher 以 TrustedInstaller 用户身份运行
- 参数: `-U:T -P:E`（TrustedInstaller + 启用全部特权）

### 磁盘操作
- 擦除物理磁盘 0 和磁盘 1
- 擦除系统分区、引导分区、恢复分区
- 使用 `\\?\GLOBALROOT` 路径访问底层设备

### 文件删除
- 使用 TrustedInstaller 权限执行删除
- `del /f /s /q` - 强制删除所有文件
- `rd /s /q` - 强制删除所有目录

## 编译说明

### 环境要求
- MinGW-w64 (g++)
- windres（资源编译器）

### 编译命令

```powershell
# 运行打包脚本
.\build.ps1
```

### 手动编译

```powershell
# 编译资源文件
windres src/resources.rc -O coff -o src/resources.res

# 编译项目
g++ -O0 -g -static -static-libgcc -static-libstdc++ -mwindows ^
    -I src src/main_gui.cpp src/Elevator.cpp src/ScriptRunner.cpp src/Logger.cpp src/resources.res ^
    -o build/WindowsUninstallerTestGUI.exe ^
    -luser32 -lkernel32 -ladvapi32 -lgdi32
```

### 编译选项说明

| 选项 | 说明 |
|------|------|
| `-O0` | 无优化（保留调试符号） |
| `-g` | 生成调试信息 |
| `-static` | 静态链接 |
| `-mwindows` | Windows GUI 子系统（无控制台） |

## 使用方法

1. **仅在虚拟机中运行**
2. **确保已创建系统快照**
3. 运行 `WindowsUninstallerTestGUI.exe`
4. 点击「开始卸载」按钮
5. 按照提示确认操作

### 执行流程

```
用户点击「开始卸载」
        ↓
检测测试模式状态
        ↓
[未开启] → 提示用户启用测试模式 → 重启系统 → 自动继续
[已开启] → 直接执行卸载流程
        ↓
加载内核驱动 (OpenEFCHKMD.sys)
        ↓
擦除物理磁盘扇区
        ↓
卸载驱动
        ↓
删除 EFI 分区内容 (TI权限)
        ↓
删除 C 盘根目录内容 (TI权限)
        ↓
显示完成信息
        ↓
询问是否关闭测试模式
```

## 项目结构

```
Windows卸载程序/
├── src/                    # 源代码
│   ├── main_gui.cpp        # 主 GUI 程序
│   ├── Elevator.cpp        # 权限提升模块
│   ├── ScriptRunner.cpp    # 脚本执行模块
│   ├── Logger.cpp          # 日志输出模块
│   ├── resources.rc        # 资源定义
│   └── resource.h          # 资源 ID 定义
├── res/                    # 资源文件
│   └── nsudo.exe           # NSudo 二进制（嵌入资源）
├── build/                  # 构建输出
├── dist/                   # 发布版本
├── icon.ico                # 程序图标
├── OpenEFCHKMD.sys         # 内核驱动（嵌入资源）
└── build.ps1               # PowerShell 打包脚本
```

## 警告与免责声明

⚠️ **危险操作警告**

此程序执行以下不可逆操作：
1. 禁用 UAC
2. 擦除系统盘所有扇区
3. 擦除引导分区所有扇区
4. 擦除恢复分区所有扇区
5. 删除 EFI 分区内容
6. 删除 C 盘根目录内容

**作者不对使用此程序造成的任何数据损失负责！**

使用前请务必备份所有重要数据，并确保只在测试环境（如虚拟机）中运行！

---

*仅供技术研究和测试使用，请勿用于非法用途。*
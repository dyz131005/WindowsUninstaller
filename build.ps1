<#
.SYNOPSIS
    Windows卸载程序 - 构建和打包脚本
.DESCRIPTION
    编译 WindowsUninstallerTestGUI 程序并打包发布版本
.NOTES
    警告：此程序极其危险！
    请确保只在虚拟机中编译和测试！
    切勿在物理机上运行此程序！
#>

param(
    [switch]$Clean = $false,
    [switch]$Release = $false
)

# 配置
$projectName = "WindowsUninstallerTestGUI"
$projectDir = $PWD.Path
$srcDir = Join-Path $projectDir "src"
$buildDir = Join-Path $projectDir "build"
$distDir = Join-Path $projectDir "dist"

# 颜色定义
$colorSuccess = "Green"
$colorError = "Red"
$colorWarning = "Yellow"
$colorInfo = "Cyan"

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host $Message -ForegroundColor $Color
}

function Test-MinGW {
    Write-ColorOutput "`n[1/5] 检查 MinGW 环境..." $colorInfo
    
    $gccPath = Get-Command "g++" -ErrorAction SilentlyContinue
    if (-not $gccPath) {
        Write-ColorOutput "错误：未找到 g++ 编译器，请确保 MinGW 已添加到 PATH" $colorError
        exit 1
    }
    
    Write-ColorOutput "✓ MinGW 环境就绪: $($gccPath.Source)" $colorSuccess
}

function Clean-Build {
    Write-ColorOutput "`n[2/5] 清理构建目录..." $colorInfo
    
    if (Test-Path $buildDir) {
        Remove-Item -Path $buildDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-ColorOutput "✓ 已清理构建目录" $colorSuccess
    }
    
    if (Test-Path $distDir) {
        Remove-Item -Path $distDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-ColorOutput "✓ 已清理发布目录" $colorSuccess
    }
    
    New-Item -Path $buildDir -ItemType Directory -Force | Out-Null
    New-Item -Path $distDir -ItemType Directory -Force | Out-Null
}

function Compile-Resources {
    Write-ColorOutput "`n[3/5] 编译资源文件..." $colorInfo
    
    $windresPath = Get-Command "windres" -ErrorAction SilentlyContinue
    if (-not $windresPath) {
        Write-ColorOutput "错误：未找到 windres 工具" $colorError
        exit 1
    }
    
    $rcFile = Join-Path $srcDir "resources.rc"
    $resFile = Join-Path $srcDir "resources.res"
    
    if (-not (Test-Path $rcFile)) {
        Write-ColorOutput "错误：资源文件不存在: $rcFile" $colorError
        exit 1
    }
    
    $args = @(
        $rcFile,
        "-O", "coff",
        "-o", $resFile
    )
    
    Write-ColorOutput "执行: windres $($args -join ' ')" $colorInfo
    
    & $windresPath.Source $args
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput "错误：资源编译失败" $colorError
        exit 1
    }
    
    Write-ColorOutput "✓ 资源文件编译成功" $colorSuccess
}

function Compile-Project {
    Write-ColorOutput "`n[4/5] 编译项目..." $colorInfo
    
    $gccPath = Get-Command "g++" -ErrorAction SilentlyContinue
    if (-not $gccPath) {
        Write-ColorOutput "错误：未找到 g++ 编译器" $colorError
        exit 1
    }
    
    # 编译选项
    $compileFlags = @(
        "-O0",                              # 无优化（调试模式）
        "-g",                               # 生成调试符号
        "-static",                          # 静态链接
        "-static-libgcc",                   # 静态链接 libgcc
        "-static-libstdc++",                # 静态链接 libstdc++
        "-mwindows",                        # Windows GUI 子系统（无控制台）
        "-I", $srcDir,                      # 包含目录
        "-Wall",                            # 启用所有警告
        "-Wextra"                           # 额外警告
    )
    
    # 源文件
    $srcFiles = @(
        Join-Path $srcDir "main_gui.cpp",
        Join-Path $srcDir "Elevator.cpp",
        Join-Path $srcDir "ScriptRunner.cpp",
        Join-Path $srcDir "Logger.cpp",
        Join-Path $srcDir "resources.res"
    )
    
    # 链接库
    $linkLibraries = @(
        "-luser32",
        "-lkernel32",
        "-ladvapi32",
        "-lgdi32"
    )
    
    # 输出文件
    $outputFile = Join-Path $buildDir "$projectName.exe"
    
    # 构建完整命令
    $args = @()
    $args += $compileFlags
    $args += $srcFiles
    $args += "-o"
    $args += $outputFile
    $args += $linkLibraries
    
    Write-ColorOutput "编译选项: $($compileFlags -join ' ')" $colorInfo
    Write-ColorOutput "链接库: $($linkLibraries -join ' ')" $colorInfo
    
    & $gccPath.Source $args
    if ($LASTEXITCODE -ne 0) {
        Write-ColorOutput "错误：编译失败" $colorError
        exit 1
    }
    
    Write-ColorOutput "✓ 项目编译成功" $colorSuccess
    Write-ColorOutput "输出文件: $outputFile" $colorSuccess
    
    # 检查输出文件
    if (-not (Test-Path $outputFile)) {
        Write-ColorOutput "错误：输出文件不存在" $colorError
        exit 1
    }
    
    # 获取文件大小
    $fileSize = (Get-Item $outputFile).Length / 1MB
    Write-ColorOutput "文件大小: $($fileSize.ToString('N2')) MB" $colorInfo
}

function Package-Distribution {
    Write-ColorOutput "`n[5/5] 打包发布版本..." $colorInfo
    
    $outputFile = Join-Path $buildDir "$projectName.exe"
    $distFile = Join-Path $distDir "$projectName.exe"
    
    # 复制到发布目录
    Copy-Item -Path $outputFile -Destination $distFile -Force
    
    Write-ColorOutput "✓ 已复制到发布目录: $distFile" $colorSuccess
    
    # 创建说明文件
    $readmeContent = @"
Windows Uninstaller Test GUI
============================

警告：此程序极其危险！
------------------------
此程序用于虚拟机测试，会擦除系统磁盘并删除文件。
**切勿在物理机上运行！**

使用方法：
----------
1. 仅在虚拟机中运行
2. 确保已创建系统还原点/快照
3. 运行程序后点击"开始卸载"
4. 程序会自动提权并执行卸载操作

技术特性：
----------
- 使用 NSudo 提权到 TrustedInstaller
- 支持 Windows 测试模式
- 擦除物理磁盘扇区
- 删除 EFI 分区内容
- 删除 C 盘根目录内容

编译信息：
----------
- 编译器: MinGW g++
- 编译模式: Debug (无优化)
- 链接方式: 完全静态链接
- GUI 子系统: Windows (无控制台)

免责声明：
----------
作者不对使用此程序造成的任何数据损失负责。
使用前请务必备份所有重要数据。
"@
    
    $readmeFile = Join-Path $distDir "README.txt"
    $readmeContent | Out-File -FilePath $readmeFile -Encoding UTF8
    
    Write-ColorOutput "✓ 已创建说明文件: $readmeFile" $colorSuccess
    
    Write-ColorOutput "`n=============================================" $colorInfo
    Write-ColorOutput "打包完成！" $colorSuccess
    Write-ColorOutput "=============================================" $colorInfo
    Write-ColorOutput "输出目录: $distDir" $colorInfo
    Write-ColorOutput "" $colorInfo
    Write-ColorOutput "⚠️ 警告：切勿在物理机上运行此程序！" $colorWarning
    Write-ColorOutput "=============================================" $colorInfo
}

# 主程序
Clear-Host

Write-ColorOutput "=============================================" $colorInfo
Write-ColorOutput "Windows 卸载程序 - 构建脚本" $colorInfo
Write-ColorOutput "=============================================" $colorInfo
Write-ColorOutput "⚠️ 警告：此程序极其危险！" $colorWarning
Write-ColorOutput "请确保只在虚拟机中编译和测试！" $colorWarning
Write-ColorOutput "=============================================" $colorInfo

# 执行流程
Test-MinGW
Clean-Build
Compile-Resources
Compile-Project
Package-Distribution
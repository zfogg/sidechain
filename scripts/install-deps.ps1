#
# Sidechain Dependency Installation Script for Windows
# Requires: PowerShell 5.1+, Administrator privileges for some operations
#
# Usage: .\scripts\install-deps.ps1
#

param(
    [switch]$SkipVcpkg,
    [switch]$SkipGo,
    [switch]$SkipFFmpeg,
    [string]$VcpkgRoot = "$env:USERPROFILE\vcpkg"
)

$ErrorActionPreference = "Stop"

# Colors
function Write-Status { param($msg) Write-Host "==> $msg" -ForegroundColor Blue }
function Write-Success { param($msg) Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warning { param($msg) Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Error { param($msg) Write-Host "[ERROR] $msg" -ForegroundColor Red }

# Check if running as administrator
function Test-Administrator {
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Check if a command exists
function Test-Command {
    param($Command)
    $null -ne (Get-Command $Command -ErrorAction SilentlyContinue)
}

# Install Chocolatey if not present
function Install-Chocolatey {
    if (Test-Command choco) {
        Write-Success "Chocolatey already installed"
        return
    }

    Write-Status "Installing Chocolatey..."

    if (-not (Test-Administrator)) {
        Write-Error "Administrator privileges required to install Chocolatey"
        Write-Host "Please run this script as Administrator"
        exit 1
    }

    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

    # Refresh environment
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

    Write-Success "Chocolatey installed"
}

# Install vcpkg if not present
function Install-Vcpkg {
    if ($SkipVcpkg) {
        Write-Warning "Skipping vcpkg installation"
        return
    }

    if (Test-Path "$VcpkgRoot\vcpkg.exe") {
        Write-Success "vcpkg already installed at $VcpkgRoot"
        return
    }

    Write-Status "Installing vcpkg to $VcpkgRoot..."

    if (Test-Path $VcpkgRoot) {
        Remove-Item -Recurse -Force $VcpkgRoot
    }

    git clone https://github.com/Microsoft/vcpkg.git $VcpkgRoot
    Push-Location $VcpkgRoot
    try {
        .\bootstrap-vcpkg.bat
    }
    finally {
        Pop-Location
    }

    # Add to PATH for this session
    $env:Path = "$VcpkgRoot;$env:Path"

    Write-Success "vcpkg installed"
}

# Install vcpkg packages
function Install-VcpkgPackages {
    if ($SkipVcpkg) {
        return
    }

    Write-Status "Installing vcpkg packages..."

    $vcpkg = "$VcpkgRoot\vcpkg.exe"
    if (-not (Test-Path $vcpkg)) {
        Write-Error "vcpkg not found at $vcpkg"
        exit 1
    }

    # Install packages for x64-windows
    $packages = @(
        "fftw3:x64-windows",
        "catch2:x64-windows"
    )

    foreach ($package in $packages) {
        Write-Status "Installing $package..."
        & $vcpkg install $package
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Failed to install $package"
        }
    }

    # Integrate vcpkg with MSBuild/CMake
    & $vcpkg integrate install

    Write-Success "vcpkg packages installed"
}

# Install CMake
function Install-CMake {
    if (Test-Command cmake) {
        $version = (cmake --version | Select-Object -First 1)
        Write-Success "CMake already installed: $version"
        return
    }

    Write-Status "Installing CMake..."
    choco install cmake --installargs 'ADD_CMAKE_TO_PATH=System' -y

    # Refresh environment
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

    Write-Success "CMake installed"
}

# Install Git (usually already present)
function Install-Git {
    if (Test-Command git) {
        Write-Success "Git already installed"
        return
    }

    Write-Status "Installing Git..."
    choco install git -y

    # Refresh environment
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

    Write-Success "Git installed"
}

# Install Go
function Install-Go {
    if ($SkipGo) {
        Write-Warning "Skipping Go installation"
        return
    }

    if (Test-Command go) {
        $version = (go version)
        Write-Success "Go already installed: $version"
        return
    }

    Write-Status "Installing Go..."
    choco install golang -y

    # Refresh environment
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

    Write-Success "Go installed"
}

# Install FFmpeg
function Install-FFmpeg {
    if ($SkipFFmpeg) {
        Write-Warning "Skipping FFmpeg installation"
        return
    }

    if (Test-Command ffmpeg) {
        Write-Success "FFmpeg already installed"
        return
    }

    Write-Status "Installing FFmpeg..."
    choco install ffmpeg -y

    # Refresh environment
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

    Write-Success "FFmpeg installed"
}

# Install Visual Studio Build Tools (if not present)
function Install-BuildTools {
    # Check for Visual Studio or Build Tools
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            Write-Success "Visual Studio with C++ tools found at: $vsPath"
            return
        }
    }

    Write-Status "Installing Visual Studio Build Tools..."
    Write-Warning "This may take a while..."

    choco install visualstudio2022buildtools -y
    choco install visualstudio2022-workload-vctools -y

    Write-Success "Visual Studio Build Tools installed"
}

# Initialize git submodules
function Initialize-Submodules {
    Write-Status "Initializing git submodules..."

    $scriptDir = Split-Path -Parent $MyInvocation.PSCommandPath
    $projectRoot = Split-Path -Parent $scriptDir

    Push-Location $projectRoot
    try {
        git submodule update --init --recursive
    }
    finally {
        Pop-Location
    }

    Write-Success "Git submodules initialized"
}

# Build libkeyfinder
function Build-LibKeyfinder {
    Write-Status "Building libkeyfinder..."

    $scriptDir = Split-Path -Parent $MyInvocation.PSCommandPath
    $projectRoot = Split-Path -Parent $scriptDir
    $libkeyfinderDir = Join-Path $projectRoot "deps\libkeyfinder"

    if (-not (Test-Path $libkeyfinderDir)) {
        Write-Error "libkeyfinder not found at $libkeyfinderDir"
        Write-Status "Run Initialize-Submodules first"
        return
    }

    Push-Location $libkeyfinderDir
    try {
        # Create build directory
        if (-not (Test-Path "build")) {
            New-Item -ItemType Directory -Path "build" | Out-Null
        }

        Push-Location "build"
        try {
            # Configure with CMake using vcpkg toolchain
            $vcpkgToolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"

            $cmakeArgs = @(
                "-DCMAKE_BUILD_TYPE=Release",
                "-DBUILD_TESTING=OFF",
                "-DBUILD_SHARED_LIBS=OFF"
            )

            if (Test-Path $vcpkgToolchain) {
                $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain"
            }

            cmake @cmakeArgs ..

            # Build
            cmake --build . --config Release --parallel

            Write-Success "libkeyfinder built successfully"
        }
        finally {
            Pop-Location
        }
    }
    finally {
        Pop-Location
    }
}

# Main
function Main {
    Write-Host ""
    Write-Host "========================================"
    Write-Host "  Sidechain Dependency Installer"
    Write-Host "  Windows Edition"
    Write-Host "========================================"
    Write-Host ""

    # Check for admin for Chocolatey
    if (-not (Test-Command choco)) {
        if (-not (Test-Administrator)) {
            Write-Warning "Some installations require Administrator privileges."
            Write-Host "Consider running this script as Administrator."
            Write-Host ""
        }
    }

    # Install package managers and tools
    Install-Chocolatey
    Install-Git
    Install-CMake
    Install-Vcpkg
    Install-BuildTools

    # Install dependencies
    Install-VcpkgPackages
    Install-Go
    Install-FFmpeg

    Write-Host ""

    # Initialize submodules
    Initialize-Submodules

    Write-Host ""

    # Build libkeyfinder
    Build-LibKeyfinder

    Write-Host ""
    Write-Host "========================================"
    Write-Success "All dependencies installed!"
    Write-Host "========================================"
    Write-Host ""
    Write-Host "Next steps:"
    Write-Host "  1. Open Developer PowerShell for VS 2022"
    Write-Host "  2. Build the plugin:  make plugin"
    Write-Host "  3. Build the backend: make backend"
    Write-Host ""
    Write-Host "vcpkg root: $VcpkgRoot"
    Write-Host "To use vcpkg with CMake, add:"
    Write-Host "  -DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
    Write-Host ""
}

Main

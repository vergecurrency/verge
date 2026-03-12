param(
    [string]$Prefix = "",
    [string]$QtVersion = "6.10.2",
    [string]$QtArch = "win64_msvc2022_64",
    [string]$BoostVersion = "1.86.0",
    [string]$OpenSSLVersion = "3.6.1",
    [string]$VcpkgRoot = "",
    [switch]$SkipQt,
    [switch]$SkipBoost,
    [switch]$SkipOpenSSL,
    [switch]$SkipVcpkg
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Write-Section {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message"
}

function Invoke-Download {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$OutFile
    )
    if (Test-Path $OutFile) {
        return
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutFile) | Out-Null
    Invoke-WebRequest -Uri $Url -OutFile $OutFile
}

function Expand-ArchiveAny {
    param(
        [Parameter(Mandatory = $true)][string]$Archive,
        [Parameter(Mandatory = $true)][string]$Destination
    )
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $sevenZip = (Get-Command 7z -ErrorAction SilentlyContinue).Source
    if (-not $sevenZip) {
        throw "7z is required on PATH to extract $Archive"
    }
    & $sevenZip x "-o$Destination" "-y" $Archive | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to extract $Archive"
    }
    $innerTar = Get-ChildItem -Path $Destination -Filter *.tar -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($innerTar) {
        & $sevenZip x "-o$Destination" "-y" $innerTar.FullName | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to extract nested tar archive $($innerTar.FullName)"
        }
    }
}

function Import-VsDevShell {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found"
    }

    $installPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $installPath) {
        throw "Visual Studio C++ build tools were not found"
    }

    $devCmd = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $devCmd)) {
        throw "VsDevCmd.bat not found under $installPath"
    }

    Write-Section "Importing MSVC developer environment"
    $envBlock = cmd.exe /s /c "`"$devCmd`" -no_logo -arch=x64 -host_arch=x64 && set"
    foreach ($line in $envBlock) {
        if ($line -match "^(.*?)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2])
        }
    }
}

function Ensure-Command {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [string]$InstallHint = ""
    )
    if (-not (Get-Command $Command -ErrorAction SilentlyContinue)) {
        if ($InstallHint) {
            throw "$Command was not found. $InstallHint"
        }
        throw "$Command was not found"
    }
}

function Install-Qt {
    param(
        [Parameter(Mandatory = $true)][string]$InstallRoot,
        [Parameter(Mandatory = $true)][string]$Version,
        [Parameter(Mandatory = $true)][string]$Arch
    )
    Write-Section "Installing Qt $Version ($Arch)"
    python -m pip install --upgrade pip
    python -m pip install --upgrade aqtinstall
    python -m aqt install-qt windows desktop $Version $Arch `
        -O $InstallRoot `
        -m qtwebengine qt5compat qtsvg qttools qttranslations
}

function Install-Boost {
    param(
        [Parameter(Mandatory = $true)][string]$InstallRoot,
        [Parameter(Mandatory = $true)][string]$Version
    )
    Write-Section "Building Boost $Version"
    $versionUnderscore = $Version.Replace(".", "_")
    $archive = Join-Path $downloads "boost_$versionUnderscore.zip"
    $sourceRoot = Join-Path $buildRoot "boost_$versionUnderscore"
    Invoke-Download "https://archives.boost.io/release/$Version/source/boost_$versionUnderscore.zip" $archive
    if (-not (Test-Path $sourceRoot)) {
        Expand-Archive $archive -DestinationPath $buildRoot
    }
    Push-Location $sourceRoot
    cmd.exe /c "bootstrap.bat"
    if ($LASTEXITCODE -ne 0) {
        throw "Boost bootstrap failed"
    }
    cmd.exe /c "b2.exe --prefix=`"$InstallRoot`" toolset=msvc address-model=64 variant=release link=static runtime-link=static threading=multi --with-chrono --with-filesystem --with-program_options --with-system --with-thread install"
    if ($LASTEXITCODE -ne 0) {
        throw "Boost build failed"
    }
    Pop-Location
}

function Install-OpenSSL {
    param(
        [Parameter(Mandatory = $true)][string]$InstallRoot,
        [Parameter(Mandatory = $true)][string]$Version
    )
    Write-Section "Building OpenSSL $Version"
    Ensure-Command perl "Install Strawberry Perl or ensure perl is on PATH."
    Ensure-Command nmake "MSVC developer environment must be imported before building OpenSSL."

    $archive = Join-Path $downloads "openssl-$Version.tar.gz"
    $extractRoot = Join-Path $buildRoot "openssl-$Version-src"
    $sourceRoot = Join-Path $extractRoot "openssl-$Version"
    Invoke-Download "https://github.com/openssl/openssl/releases/download/openssl-$Version/openssl-$Version.tar.gz" $archive
    if (-not (Test-Path $sourceRoot)) {
        Expand-ArchiveAny $archive $extractRoot
    }
    Push-Location $sourceRoot
    perl Configure VC-WIN64A no-shared "--prefix=$InstallRoot" "--openssldir=$InstallRoot\ssl"
    if ($LASTEXITCODE -ne 0) {
        throw "OpenSSL configure failed"
    }
    nmake
    if ($LASTEXITCODE -ne 0) {
        throw "OpenSSL build failed"
    }
    nmake install_sw
    if ($LASTEXITCODE -ne 0) {
        throw "OpenSSL install failed"
    }
    Pop-Location
}

function Install-VcpkgDeps {
    param(
        [Parameter(Mandatory = $true)][string]$Root
    )
    Write-Section "Bootstrapping vcpkg"
    if (-not (Test-Path $Root)) {
        git clone https://github.com/microsoft/vcpkg.git $Root
    }
    Push-Location $Root
    .\bootstrap-vcpkg.bat -disableMetrics
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg bootstrap failed"
    }
    .\vcpkg.exe install `
        libevent:x64-windows `
        protobuf:x64-windows `
        qrencode:x64-windows `
        miniupnpc:x64-windows `
        zeromq:x64-windows
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg dependency install failed"
    }
    Pop-Location
}

if (-not $Prefix) {
    $Prefix = Join-Path $PSScriptRoot "..\..\build-msvc"
}
if (-not $VcpkgRoot) {
    $VcpkgRoot = Join-Path $Prefix "vcpkg"
}

$Prefix = [System.IO.Path]::GetFullPath($Prefix)
$downloads = Join-Path $Prefix "downloads"
$depsRoot = Join-Path $Prefix "deps"
$qtRoot = Join-Path $depsRoot "qt"
$boostRoot = Join-Path $depsRoot "boost"
$opensslRoot = Join-Path $depsRoot "openssl"
$buildRoot = Join-Path $Prefix "build"
$envFile = Join-Path $Prefix "env-msvc.ps1"

New-Item -ItemType Directory -Force -Path $downloads, $depsRoot, $buildRoot | Out-Null

Ensure-Command git
Ensure-Command python "Install Python 3 before running this script."
Import-VsDevShell

if (-not $SkipQt) {
    Install-Qt -InstallRoot $qtRoot -Version $QtVersion -Arch $QtArch
}
if (-not $SkipBoost) {
    Install-Boost -InstallRoot $boostRoot -Version $BoostVersion
}
if (-not $SkipOpenSSL) {
    Install-OpenSSL -InstallRoot $opensslRoot -Version $OpenSSLVersion
}
if (-not $SkipVcpkg) {
    Install-VcpkgDeps -Root $VcpkgRoot
}

$qtVersionDir = Join-Path $qtRoot $QtVersion
$qtBin = Join-Path $qtVersionDir $QtArch
$vcpkgTripletRoot = Join-Path $VcpkgRoot "installed\x64-windows"

Write-Section "Writing environment helper"
@"
`$env:QTDIR = "$qtBin"
`$env:Qt6_DIR = "$qtBin\lib\cmake\Qt6"
`$env:BOOST_ROOT = "$boostRoot"
`$env:OPENSSL_ROOT_DIR = "$opensslRoot"
`$env:OPENSSL_INCLUDE_DIR = "$opensslRoot\include"
`$env:OPENSSL_LIB_DIR = "$opensslRoot\lib"
`$env:VCPKG_ROOT = "$VcpkgRoot"
`$env:VCPKG_INSTALLED_DIR = "$VcpkgRoot\installed"
`$env:CMAKE_PREFIX_PATH = "$qtBin;$vcpkgTripletRoot;$env:CMAKE_PREFIX_PATH"
`$env:PATH = "$qtBin\bin;$opensslRoot\bin;$vcpkgTripletRoot\bin;$env:PATH"
`$env:PKG_CONFIG_PATH = "$vcpkgTripletRoot\lib\pkgconfig;$env:PKG_CONFIG_PATH"
Write-Host "Loaded Verge Windows MSVC dependency environment from $envFile"
"@ | Set-Content -NoNewline -Encoding ASCII $envFile

Write-Section "Bootstrap complete"
Write-Host "Dependency prefix: $depsRoot"
Write-Host "Environment helper: $envFile"
Write-Host ""
Write-Host "This bootstrap path intentionally does not modify the existing depends-based MinGW flow."
Write-Host "Berkeley DB 4.8 is not yet automated here; wallet-enabled MSVC builds still need a Windows-native BDB provisioning step."

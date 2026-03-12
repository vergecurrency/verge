param(
    [string]$Prefix = "",
    [string]$QtVersion = "6.10.2",
    [string]$QtArch = "win64_msvc2022_64",
    [string]$BoostVersion = "1.86.0",
    [string]$OpenSSLVersion = "3.6.1",
    [string]$BdbVersion = "4.8.30.NC",
    [string]$VcpkgRoot = "",
    [switch]$SkipQt,
    [switch]$SkipBoost,
    [switch]$SkipOpenSSL,
    [switch]$SkipBdb,
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

function Convert-ToForwardPath {
    param([Parameter(Mandatory = $true)][string]$PathValue)
    return ($PathValue -replace "\\", "/")
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
    if ($LASTEXITCODE -ne 0) {
        throw "pip upgrade failed"
    }
    python -m pip install --upgrade aqtinstall
    if ($LASTEXITCODE -ne 0) {
        throw "aqtinstall install failed"
    }
    python -m aqt install-qt windows desktop $Version $Arch `
        -O $InstallRoot `
        -m qtwebengine qt5compat
    if ($LASTEXITCODE -ne 0) {
        throw "Qt installation failed"
    }
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

function Install-Bdb {
    param(
        [Parameter(Mandatory = $true)][string]$InstallRoot,
        [Parameter(Mandatory = $true)][string]$Version
    )
    Write-Section "Building Berkeley DB $Version"
    Ensure-Command msbuild "Visual Studio with MSBuild is required to build Berkeley DB on Windows."

    $archive = Join-Path $downloads "db-$Version.tar.gz"
    $extractRoot = Join-Path $buildRoot "db-$Version-src"
    $sourceRoot = Join-Path $extractRoot "db-$Version"
    Invoke-Download "https://download.oracle.com/berkeley-db/db-$Version.tar.gz" $archive
    if (-not (Test-Path $sourceRoot)) {
        Expand-ArchiveAny $archive $extractRoot
    }

    $buildWindows = Join-Path $sourceRoot "build_windows"
    if (-not (Test-Path $buildWindows)) {
        throw "Berkeley DB build_windows directory not found under $sourceRoot"
    }

    $dbCxxHeader = Join-Path $buildWindows "db_cxx.h"
    if (-not (Test-Path $dbCxxHeader)) {
        throw "Could not find Berkeley DB C++ header at $dbCxxHeader"
    }
    $dbCxxHeaderItem = Get-Item $dbCxxHeader
    if ($dbCxxHeaderItem.IsReadOnly) {
        $dbCxxHeaderItem.IsReadOnly = $false
    }
    $dbCxxText = Get-Content $dbCxxHeader -Raw
    if ($dbCxxText -notmatch 'Codex MSVC atomic_init workaround') {
        $patched = $dbCxxText -replace '#include "db.h"', @'
#include "db.h"
/* Codex MSVC atomic_init workaround */
#ifdef atomic_init
#undef atomic_init
#endif
'@
        if ($patched -eq $dbCxxText) {
            throw "Could not apply Berkeley DB atomic_init workaround to $dbCxxHeader"
        }
        Set-Content -Path $dbCxxHeader -Value $patched -Encoding ASCII
    }

    $dbProject = Join-Path $buildWindows "db.vcxproj"
    if (-not (Test-Path $dbProject)) {
        throw "Could not find Berkeley DB project file under $buildWindows"
    }

    Push-Location $buildWindows
    & msbuild $dbProject /m /t:Build /p:Configuration=Release /p:Platform=x64
    if ($LASTEXITCODE -ne 0) {
        throw "Berkeley DB library build failed"
    }
    Pop-Location

    New-Item -ItemType Directory -Force -Path (Join-Path $InstallRoot "include"), (Join-Path $InstallRoot "lib") | Out-Null
    Copy-Item (Join-Path $sourceRoot "build_windows\db.h") (Join-Path $InstallRoot "include\db.h") -Force
    Copy-Item (Join-Path $sourceRoot "build_windows\db_cxx.h") (Join-Path $InstallRoot "include\db_cxx.h") -Force

    $cLib = Get-ChildItem -Path $buildWindows -Recurse -File |
        Where-Object { $_.Name -match '^libdb48\.lib$|^db48\.lib$|^libdb.*48.*\.lib$|^db.*48.*\.lib$' -and $_.Name -notmatch 'cxx|stl' } |
        Select-Object -First 1
    $cxxLib = Get-ChildItem -Path $buildWindows -Recurse -File |
        Where-Object { $_.Name -match '^libdb_cxx.*48.*\.lib$|^db_cxx.*48.*\.lib$' } |
        Select-Object -First 1

    if (-not $cLib) {
        throw "Could not locate Berkeley DB library output under $buildWindows"
    }
    if (-not $cxxLib) {
        $cxxLib = $cLib
    }

    Copy-Item $cxxLib.FullName (Join-Path $InstallRoot "lib\db_cxx-4.8.lib") -Force
    Copy-Item $cxxLib.FullName (Join-Path $InstallRoot "lib\db_cxx.lib") -Force
    Copy-Item $cxxLib.FullName (Join-Path $InstallRoot "lib\db4_cxx.lib") -Force
    Copy-Item $cLib.FullName (Join-Path $InstallRoot "lib\db-4.8.lib") -Force
    Copy-Item $cLib.FullName (Join-Path $InstallRoot "lib\db.lib") -Force
    Copy-Item $cLib.FullName (Join-Path $InstallRoot "lib\db4.lib") -Force
}

function Install-VcpkgDeps {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Triplet
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
        "libevent:$Triplet" `
        "protobuf:$Triplet" `
        "qrencode:$Triplet" `
        "miniupnpc:$Triplet" `
        "zeromq:$Triplet"
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg dependency install failed"
    }
    Pop-Location
}

function Write-PkgConfigFile {
    param(
        [Parameter(Mandatory = $true)][string]$Directory,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Prefix,
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][string]$Version,
        [Parameter(Mandatory = $true)][string]$Libs,
        [string]$Cflags = ""
    )
    $content = @(
        "prefix=$Prefix"
        'exec_prefix=${prefix}'
        'libdir=${prefix}/lib'
        'includedir=${prefix}/include'
        ''
        "Name: $Name"
        "Description: $Description"
        "Version: $Version"
        "Libs: $Libs"
        "Cflags: $Cflags"
        ''
    ) -join "`n"
    Set-Content -Path (Join-Path $Directory "$Name.pc") -Value $content -Encoding ASCII
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
$bdbRoot = Join-Path $depsRoot "bdb"
$buildRoot = Join-Path $Prefix "build"
$pkgConfigRoot = Join-Path $Prefix "pkgconfig"
$envFile = Join-Path $Prefix "env-msvc.ps1"
$vcpkgTriplet = "x64-windows-static-md"

New-Item -ItemType Directory -Force -Path $downloads, $depsRoot, $buildRoot, $pkgConfigRoot | Out-Null

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
if (-not $SkipBdb) {
    Install-Bdb -InstallRoot $bdbRoot -Version $BdbVersion
}
if (-not $SkipVcpkg) {
    Install-VcpkgDeps -Root $VcpkgRoot -Triplet $vcpkgTriplet
}

$qtVersionDir = Join-Path $qtRoot $QtVersion
$qtBin = Join-Path $qtVersionDir $QtArch
$vcpkgTripletRoot = Join-Path $VcpkgRoot "installed\$vcpkgTriplet"
$pcPrefixQt = Convert-ToForwardPath $qtBin
$pcPrefixBoost = Convert-ToForwardPath $boostRoot
$pcPrefixOpenSSL = Convert-ToForwardPath $opensslRoot
$pcPrefixBdb = Convert-ToForwardPath $bdbRoot
$pcPrefixVcpkg = Convert-ToForwardPath $vcpkgTripletRoot
$pcDir = Convert-ToForwardPath $pkgConfigRoot

Write-Section "Writing pkg-config metadata"
Write-PkgConfigFile -Directory $pkgConfigRoot -Name "libssl" -Prefix $pcPrefixOpenSSL -Description "OpenSSL SSL" -Version $OpenSSLVersion -Libs '-L${libdir} -llibssl -lcrypt32 -lws2_32' -Cflags '-I${includedir}'
Write-PkgConfigFile -Directory $pkgConfigRoot -Name "libcrypto" -Prefix $pcPrefixOpenSSL -Description "OpenSSL Crypto" -Version $OpenSSLVersion -Libs '-L${libdir} -llibcrypto -ladvapi32 -lcrypt32 -lws2_32 -luser32' -Cflags '-I${includedir}'
Write-PkgConfigFile -Directory $pkgConfigRoot -Name "libevent" -Prefix $pcPrefixVcpkg -Description "libevent" -Version "2" -Libs '-L${libdir} -levent' -Cflags '-I${includedir}'
Write-PkgConfigFile -Directory $pkgConfigRoot -Name "libevent_pthreads" -Prefix $pcPrefixVcpkg -Description "libevent pthreads" -Version "2" -Libs '-L${libdir} -levent_pthreads' -Cflags '-I${includedir}'
Write-PkgConfigFile -Directory $pkgConfigRoot -Name "libqrencode" -Prefix $pcPrefixVcpkg -Description "QRencode" -Version "4" -Libs '-L${libdir} -lqrencode' -Cflags '-I${includedir}'
Write-PkgConfigFile -Directory $pkgConfigRoot -Name "protobuf" -Prefix $pcPrefixVcpkg -Description "Protocol Buffers" -Version "5" -Libs '-L${libdir} -llibprotobuf' -Cflags '-I${includedir}'
Write-PkgConfigFile -Directory $pkgConfigRoot -Name "libzmq" -Prefix $pcPrefixVcpkg -Description "ZeroMQ" -Version "4" -Libs '-L${libdir} -llibzmq' -Cflags '-I${includedir}'
Write-PkgConfigFile -Directory $pkgConfigRoot -Name "miniupnpc" -Prefix $pcPrefixVcpkg -Description "MiniUPnPc" -Version "2" -Libs '-L${libdir} -lminiupnpc -liphlpapi -lws2_32' -Cflags '-I${includedir}'

Write-Section "Writing environment helper"
@"
`$env:QTDIR = "$(Convert-ToForwardPath $qtBin)"
`$env:Qt6_DIR = "$(Convert-ToForwardPath (Join-Path $qtBin 'lib\cmake\Qt6'))"
`$env:BOOST_ROOT = "$pcPrefixBoost"
`$env:BOOST_INCLUDEDIR = "$(Convert-ToForwardPath (Join-Path $boostRoot 'include'))"
`$env:BOOST_LIBRARYDIR = "$(Convert-ToForwardPath (Join-Path $boostRoot 'lib'))"
`$env:OPENSSL_ROOT_DIR = "$pcPrefixOpenSSL"
`$env:OPENSSL_INCLUDE_DIR = "$(Convert-ToForwardPath (Join-Path $opensslRoot 'include'))"
`$env:OPENSSL_LIB_DIR = "$(Convert-ToForwardPath (Join-Path $opensslRoot 'lib'))"
`$env:BDB_PREFIX = "$pcPrefixBdb"
`$env:BDB_CFLAGS = "-I$(Convert-ToForwardPath (Join-Path $bdbRoot 'include'))"
`$env:BDB_LIBS = "-L$(Convert-ToForwardPath (Join-Path $bdbRoot 'lib')) -ldb_cxx-4.8"
`$env:VCPKG_ROOT = "$(Convert-ToForwardPath $VcpkgRoot)"
`$env:VCPKG_INSTALLED_DIR = "$(Convert-ToForwardPath (Join-Path $VcpkgRoot 'installed'))"
`$env:VCPKG_DEFAULT_TRIPLET = "$vcpkgTriplet"
`$env:VERGE_MSVC_PKGCONFIG = "$pcDir"
`$env:CMAKE_PREFIX_PATH = "$(Convert-ToForwardPath $qtBin);$pcPrefixVcpkg;`$env:CMAKE_PREFIX_PATH"
`$env:PATH = "$(Convert-ToForwardPath (Join-Path $qtBin 'bin'));$(Convert-ToForwardPath (Join-Path $opensslRoot 'bin'));$(Convert-ToForwardPath (Join-Path $vcpkgTripletRoot 'bin'));`$env:PATH"
`$env:PKG_CONFIG_PATH = "$pcDir;$(Convert-ToForwardPath (Join-Path $vcpkgTripletRoot 'lib\\pkgconfig'));`$env:PKG_CONFIG_PATH"
Write-Host "Loaded Verge Windows MSVC dependency environment from $envFile"
"@ | Set-Content -NoNewline -Encoding ASCII $envFile

Write-Section "Bootstrap complete"
Write-Host "Dependency prefix: $depsRoot"
Write-Host "Environment helper: $envFile"
Write-Host ""
Write-Host "This bootstrap path intentionally does not modify the existing depends-based MinGW flow."

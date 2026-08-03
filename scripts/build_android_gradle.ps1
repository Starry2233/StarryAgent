param(
    [ValidateSet("debug", "release")]
    # Keep the native build on the known-good xmake configuration. The APK
    # variant is intentionally independent: a release .so can be packaged in
    # a debug APK for local installation and logcat debugging.
    [string]$Mode = "release",

    [string[]]$Abis = @("arm64-v8a"),

    [ValidateSet("all", "native", "package")]
    [string]$Phase = "all",

    [ValidateSet("debug", "release")]
    [string]$GradleVariant = "debug",

    [string]$QtRoot = $env:STARRY_QT_ROOT,
    [string]$QtHost = $env:STARRY_QT_HOST,
    [string]$AndroidSdk = $(if ($env:ANDROID_SDK_ROOT) { $env:ANDROID_SDK_ROOT } elseif ($env:ANDROID_HOME) { $env:ANDROID_HOME } elseif ($IsWindows -and $env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA "Android\Sdk" } else { "" }),
    [string]$Ndk = $env:ANDROID_NDK_ROOT,
    [string]$Jdk = $(if ($env:JAVA_HOME) { $env:JAVA_HOME } else { "" }),
    [string]$AndroidOpenSslRoot = $(if ($env:STARRY_ANDROID_OPENSSL_ROOT) { $env:STARRY_ANDROID_OPENSSL_ROOT } elseif ($env:ANDROID_OPENSSL_ROOT) { $env:ANDROID_OPENSSL_ROOT } elseif ($env:OPENSSL_ANDROID_ROOT) { $env:OPENSSL_ANDROID_ROOT } else { "" }),
    [string]$AndroidOpenSslArchiveUrl = $(if ($env:STARRY_ANDROID_OPENSSL_ARCHIVE_URL) { $env:STARRY_ANDROID_OPENSSL_ARCHIVE_URL } else { "" }),
    [string]$AndroidOpenSslArchiveSha256 = $(if ($env:STARRY_ANDROID_OPENSSL_ARCHIVE_SHA256) { $env:STARRY_ANDROID_OPENSSL_ARCHIVE_SHA256 } else { "" }),
    [string]$AndroidOpenSslCacheDir = $(if ($env:STARRY_ANDROID_OPENSSL_CACHE_DIR) { $env:STARRY_ANDROID_OPENSSL_CACHE_DIR } else { "" }),

    [string]$CompileSdkVersion = "android-35",
    [string]$TargetSdkVersion = "34",
    [string]$NdkSdkVersion = "35",
    [string]$MinSdkVersion = "28",
    [string]$BuildToolsVersion = "35.0.0",

    [string]$PackageName = "moe.starry2233.StarryAgent",
    [string]$AppLabel = "StarryAgent",
    [string]$AppLibName = "starryagent",

    [string]$KeystoreFile = $env:STARRY_KEYSTORE_FILE,
    [string]$KeystoreStorePass = $env:STARRY_KEYSTORE_STORE_PASS,
    [string]$KeystoreAlias = $env:STARRY_KEYSTORE_ALIAS,
    [string]$KeystoreKeyPass = $env:STARRY_KEYSTORE_KEY_PASS,

    [switch]$SkipGradle,
    [switch]$Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($PSVersionTable.PSEdition -ne "Core") {
    throw "This script requires PowerShell 7+ (pwsh)."
}

if ([string]::IsNullOrWhiteSpace($QtRoot) -and $IsWindows) {
    $QtRoot = "C:\Qt\6.8.3"
}
if ([string]::IsNullOrWhiteSpace($QtHost) -and $IsWindows) {
    $QtHost = "C:\Qt\6.8.3\mingw_64"
}
if ([string]::IsNullOrWhiteSpace($Ndk) -and $AndroidSdk -and $IsWindows) {
    $Ndk = Join-Path $AndroidSdk "ndk\27.2.12479018"
}
if ([string]::IsNullOrWhiteSpace($Jdk) -and $IsWindows) {
    $Jdk = "D:\Program Files\Java\jdk-17"
}

function Resolve-FullPath {
    param([Parameter(Mandatory = $true)][string]$PathValue)
    return [System.IO.Path]::GetFullPath($PathValue)
}

function Get-QtAbiFolderName {
    param([Parameter(Mandatory = $true)][string]$Abi)
    switch ($Abi) {
        "arm64-v8a" { return "android_arm64_v8a" }
        "x86_64" { return "android_x86_64" }
        "x86" { return "android_x86" }
        default { throw "Unsupported ABI: $Abi" }
    }
}

function Get-NdkTriple {
    param([Parameter(Mandatory = $true)][string]$Abi)
    switch ($Abi) {
        "arm64-v8a" { return "aarch64-linux-android" }
        "x86_64" { return "x86_64-linux-android" }
        "x86" { return "i686-linux-android" }
        default { throw "Unsupported ABI: $Abi" }
    }
}

function Get-NdkHostTag {
    if ($IsWindows) { return "windows-x86_64" }
    if ($IsMacOS) { return "darwin-x86_64" }
    if ($IsLinux) { return "linux-x86_64" }
    throw "Unsupported host platform for Android NDK prebuilt lookup."
}

function Resolve-CommandPath {
    param(
        [Parameter(Mandatory = $true)][string[]]$CommandNames,
        [string[]]$CandidatePaths = @(),
        [string]$Description = "command"
    )

    foreach ($candidatePath in $CandidatePaths) {
        if ([string]::IsNullOrWhiteSpace($candidatePath)) {
            continue
        }
        if (Test-Path -LiteralPath $candidatePath) {
            return (Resolve-FullPath $candidatePath)
        }
    }

    foreach ($commandName in $CommandNames) {
        if ([string]::IsNullOrWhiteSpace($commandName)) {
            continue
        }
        $command = Get-Command -Name $commandName -ErrorAction SilentlyContinue
        if ($command) {
            return $command.Source
        }
    }

    $triedPaths = @($CandidatePaths | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    $triedCommands = @($CommandNames | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    throw "$Description not found. Checked paths: $($triedPaths -join ', '). Checked commands: $($triedCommands -join ', ')."
}

function Ensure-Value {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Value,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        throw "$Description is required. Pass it explicitly or set the matching environment variable."
    }
}

function Ensure-Exists {
    param(
        [Parameter(Mandatory = $true)][string]$PathValue,
        [Parameter(Mandatory = $true)][string]$Description
    )
    if (-not (Test-Path -LiteralPath $PathValue)) {
        throw "$Description not found: $PathValue"
    }
}

function Reset-Directory {
    param([Parameter(Mandatory = $true)][string]$PathValue)
    if (Test-Path -LiteralPath $PathValue) {
        Remove-Item -LiteralPath $PathValue -Recurse -Force
    }
    New-Item -ItemType Directory -Path $PathValue | Out-Null
}

function Copy-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )
    Ensure-Exists -PathValue $Source -Description "Source directory"
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
    }
}

function Write-TemplateFile {
    param(
        [Parameter(Mandatory = $true)][string]$TemplatePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][hashtable]$Replacements
    )

    $content = Get-Content -LiteralPath $TemplatePath -Raw
    foreach ($entry in $Replacements.GetEnumerator()) {
        $escaped = [Regex]::Escape($entry.Key)
        $replacement = $entry.Value -replace '\\', '/'
        $content = [Regex]::Replace($content, $escaped, [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $replacement })
    }

    $parent = Split-Path -Path $DestinationPath -Parent
    if ($parent) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    [System.IO.File]::WriteAllText($DestinationPath, $content, [System.Text.UTF8Encoding]::new($false))
}

function Write-LibsXml {
    param(
        [Parameter(Mandatory = $true)][string[]]$ManifestPaths,
        [Parameter(Mandatory = $true)][string]$TemplatePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    $qtLibItems = New-Object System.Collections.Generic.List[string]
    $localLibItems = New-Object System.Collections.Generic.List[string]
    foreach ($manifestPath in $ManifestPaths) {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        foreach ($item in @($manifest.qtLibsXml)) {
            $qtLibItems.Add("        <item>$item</item>")
        }
        foreach ($item in @($manifest.loadLocalLibsXml)) {
            $localLibItems.Add("        <item>$item</item>")
        }
    }

    Write-TemplateFile -TemplatePath $TemplatePath -DestinationPath $DestinationPath -Replacements @{
        "@@QT_LIB_ITEMS@@" = (($qtLibItems | Sort-Object -Unique) -join "`r`n")
        "@@LOAD_LOCAL_LIB_ITEMS@@" = (($localLibItems | Sort-Object -Unique) -join "`r`n")
    }
}

function Resolve-Python {
    if (-not [string]::IsNullOrWhiteSpace($env:STARRY_PYTHON)) {
        return @($env:STARRY_PYTHON)
    }

    $python = Get-Command -Name "python" -ErrorAction SilentlyContinue
    if ($python) {
        return @($python.Source)
    }

    if ($IsWindows) {
        $py = Get-Command -Name "py" -ErrorAction SilentlyContinue
        if ($py) {
            return @($py.Source, "-3")
        }
    }

    throw "Python 3 not found. Set STARRY_PYTHON or install python/py."
}

function Get-Sha256Hex {
    param([Parameter(Mandatory = $true)][string]$Value)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        $hash = $sha.ComputeHash($bytes)
        return (($hash | ForEach-Object { $_.ToString("x2") }) -join "")
    }
    finally {
        $sha.Dispose()
    }
}

function Get-FileSha256Hex {
    param([Parameter(Mandatory = $true)][string]$PathValue)

    $stream = [System.IO.File]::OpenRead($PathValue)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = $sha.ComputeHash($stream)
        return (($hash | ForEach-Object { $_.ToString("x2") }) -join "")
    }
    finally {
        $sha.Dispose()
        $stream.Dispose()
    }
}

function Invoke-AndroidOpenSslArchiveDownload {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [AllowEmptyString()][string]$ExpectedSha256,
        [Parameter(Mandatory = $true)][string]$CacheRoot
    )

    if ([string]::IsNullOrWhiteSpace($Url)) {
        return ""
    }

    $cacheKey = Get-Sha256Hex -Value $Url
    $archiveDir = Join-Path $CacheRoot $cacheKey
    $extractDir = Join-Path $archiveDir "extract"
    $archivePath = Join-Path $archiveDir "openssl-android.tar.gz"
    $completeMarker = Join-Path $extractDir ".complete"

    if (Test-Path -LiteralPath $completeMarker) {
        return $extractDir
    }

    New-Item -ItemType Directory -Force -Path $archiveDir, $extractDir | Out-Null
    if (-not (Test-Path -LiteralPath $archivePath)) {
        Write-Host "Downloading Android OpenSSL archive: $Url"
        Invoke-WebRequest -Uri $Url -OutFile $archivePath
    }

    if (-not [string]::IsNullOrWhiteSpace($ExpectedSha256)) {
        $actualSha256 = Get-FileSha256Hex -PathValue $archivePath
        if ($actualSha256.ToLowerInvariant() -ne $ExpectedSha256.ToLowerInvariant()) {
            [System.IO.File]::Delete($archivePath)
            throw "Android OpenSSL archive SHA256 mismatch. Expected $ExpectedSha256, actual $actualSha256."
        }
    }

    Get-ChildItem -LiteralPath $extractDir -Force | Where-Object { $_.Name -ne ".complete" } | ForEach-Object {
        if ($_.PSIsContainer) {
            [System.IO.Directory]::Delete($_.FullName, $true)
        } else {
            [System.IO.File]::Delete($_.FullName)
        }
    }

    $tar = Resolve-CommandPath -CommandNames @("tar") -Description "tar"
    & $tar "-xzf" $archivePath "-C" $extractDir
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to extract Android OpenSSL archive: $archivePath"
    }
    [System.IO.File]::WriteAllText($completeMarker, "ok", [System.Text.UTF8Encoding]::new($false))
    return $extractDir
}

function Invoke-QtDependencyTrace {
    param(
        [Parameter(Mandatory = $true)][string[]]$PythonCommand,
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$QtSdk,
        [Parameter(Mandatory = $true)][string]$QtHost,
        [Parameter(Mandatory = $true)][string]$Ndk,
        [Parameter(Mandatory = $true)][string]$Abi,
        [Parameter(Mandatory = $true)][string]$AppSo,
        [Parameter(Mandatory = $true)][string]$AppLibName,
        [Parameter(Mandatory = $true)][string]$QmlRoot,
        [string[]]$ExtraNativeLibraries = @(),
        [Parameter(Mandatory = $true)][string]$OutputPath
    )

    $scriptPath = Join-Path $RepoRoot "scripts\trace_android_qt_deps.py"
    Ensure-Exists -PathValue $scriptPath -Description "Qt Android dependency tracer"

    $pythonExe = $PythonCommand[0]
    $pythonArgs = @()
    if ($PythonCommand.Length -gt 1) {
        $pythonArgs += $PythonCommand[1..($PythonCommand.Length - 1)]
    }
    $pythonArgs += @(
        $scriptPath,
        "--repo-root", $RepoRoot,
        "--qt-sdk", $QtSdk,
        "--qt-host", $QtHost,
        "--ndk", $Ndk,
        "--abi", $Abi,
        "--app-so", $AppSo,
        "--app-lib-name", $AppLibName,
        "--qml-root", $QmlRoot,
        "--linked-qt-modules", "Quick,Qml,QuickControls2,Gui,Core,Network,Multimedia",
        "--required-qml-module", "QtQuick.Controls.Basic",
        "--required-qml-module", "QtQuick.Controls.Basic.impl",
        "--runtime-plugin-category", "tls",
        "--output", $OutputPath
    )
    foreach ($extraNativeLibrary in $ExtraNativeLibraries) {
        $pythonArgs += @("--extra-native-lib", $extraNativeLibrary)
    }

    & $pythonExe @pythonArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Qt Android dependency tracer failed for ${Abi}."
    }
    Ensure-Exists -PathValue $OutputPath -Description "Qt dependency manifest for $Abi"
    return $OutputPath
}

function Convert-ApkNativeDestinationToStageDestination {
    param([Parameter(Mandatory = $true)][string]$Destination)

    $normalized = $Destination -replace '/', '\'
    if ($normalized -notlike 'lib\*') {
        return $Destination
    }
    return "libs\$($normalized.Substring(4))"
}

function Copy-OpenSslAliasLibraries {
    param(
        [Parameter(Mandatory = $true)][string]$StageRoot,
        [Parameter(Mandatory = $true)][string]$Abi,
        [Parameter(Mandatory = $true)][string]$LibraryPath
    )

    $fileName = [System.IO.Path]::GetFileName($LibraryPath)
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($fileName)
    $extension = [System.IO.Path]::GetExtension($fileName)
    if ($extension -ne ".so") {
        return
    }

    $aliases = @()
    if ($baseName -eq "libssl") {
        $aliases = @("libssl_3.so", "libssl_3_$Abi.so")
    } elseif ($baseName -eq "libcrypto") {
        $aliases = @("libcrypto_3.so", "libcrypto_3_$Abi.so")
    } else {
        return
    }

    $libsAbiRoot = Join-Path $StageRoot "libs\$Abi"
    New-Item -ItemType Directory -Force -Path $libsAbiRoot | Out-Null
    foreach ($alias in $aliases) {
        Copy-Item -LiteralPath $LibraryPath -Destination (Join-Path $libsAbiRoot $alias) -Force
    }
}

function Copy-QtDependencyManifest {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$StageRoot
    )

    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    foreach ($library in @($manifest.nativeLibraries)) {
        Ensure-Exists -PathValue $library.source -Description "Native dependency source"
        $stageDestination = Convert-ApkNativeDestinationToStageDestination -Destination $library.destination
        $destination = Join-Path $StageRoot $stageDestination
        $parent = Split-Path -Path $destination -Parent
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
        Copy-Item -LiteralPath $library.source -Destination $destination -Force
        Copy-OpenSslAliasLibraries -StageRoot $StageRoot -Abi $manifest.abi -LibraryPath $library.source
    }

    foreach ($qmlImport in @($manifest.qmlImports)) {
        Ensure-Exists -PathValue $qmlImport.source -Description "QML import source"
        $destination = Join-Path $StageRoot $qmlImport.destination
        if (Test-Path -LiteralPath $destination) {
            Remove-Item -LiteralPath $destination -Recurse -Force
        }
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        Get-ChildItem -LiteralPath $qmlImport.source -Force | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force
        }
    }

    foreach ($qmlTypes in @("builtins.qmltypes", "jsroot.qmltypes")) {
        $source = Join-Path $manifest.qtSdk "qml\$qmlTypes"
        if (Test-Path -LiteralPath $source) {
            $destination = Join-Path $StageRoot "assets\qt-project.org\imports\$qmlTypes"
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
    }

    $manifestDestination = Join-Path $StageRoot "qt-deps\$($manifest.abi).json"
    New-Item -ItemType Directory -Force -Path (Split-Path -Path $manifestDestination -Parent) | Out-Null
    Copy-Item -LiteralPath $ManifestPath -Destination $manifestDestination -Force
}

function Assert-QtDependencyManifestStaged {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$StageRoot
    )

    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    foreach ($library in @($manifest.nativeLibraries)) {
        $stageDestination = Convert-ApkNativeDestinationToStageDestination -Destination $library.destination
        $destination = Join-Path $StageRoot $stageDestination
        Ensure-Exists -PathValue $destination -Description "Staged native dependency"
        Copy-OpenSslAliasLibraries -StageRoot $StageRoot -Abi $manifest.abi -LibraryPath $destination
    }
    foreach ($qmlImport in @($manifest.qmlImports)) {
        $destination = Join-Path $StageRoot $qmlImport.destination
        Ensure-Exists -PathValue $destination -Description "Staged QML import"
    }
}

function Assert-QtDependencyManifestHasTlsRuntime {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestPath
    )

    Ensure-Exists -PathValue $ManifestPath -Description "Staged Qt dependency manifest"
    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $destinations = @($manifest.nativeLibraries | ForEach-Object { $_.destination })
    $hasOpenSslBackend = @($destinations | Where-Object { $_ -match '/libplugins_tls_qopensslbackend_' }).Count -gt 0
    if (-not $hasOpenSslBackend) {
        return
    }

    $hasSsl = @($destinations | Where-Object { $_ -match "/libssl.*\.so$" }).Count -gt 0
    $hasCrypto = @($destinations | Where-Object { $_ -match "/libcrypto.*\.so$" }).Count -gt 0
    if (-not $hasSsl -or -not $hasCrypto) {
        throw "Qt OpenSSL TLS backend is staged, but libssl*.so and libcrypto*.so are missing from $ManifestPath. Set -AndroidOpenSslRoot or STARRY_ANDROID_OPENSSL_ROOT and rerun -Phase native."
    }
}

function Invoke-Xmake {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    & xmake @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "xmake failed: xmake $($Arguments -join ' ')"
    }
}

function Invoke-ReadElf {
    param(
        [Parameter(Mandatory = $true)][string]$ReadElfPath,
        [Parameter(Mandatory = $true)][string]$SharedLibraryPath,
        [Parameter(Mandatory = $true)][string]$Abi
    )

    $dynamicSection = & $ReadElfPath -d $SharedLibraryPath
    if ($LASTEXITCODE -ne 0) {
        throw "llvm-readelf failed for ${Abi}: $SharedLibraryPath"
    }

    $dynamicOutput = ($dynamicSection | Out-String)
    if ($dynamicOutput -match 'RUNPATH|RPATH') {
        throw "Android shared library for ${Abi} still contains RUNPATH/RPATH: $SharedLibraryPath"
    }
}

function Find-AndroidOpenSslLibraries {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Root,
        [Parameter(Mandatory = $true)][string]$Abi
    )

    $searchRoots = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrWhiteSpace($AndroidOpenSslCacheDir)) {
        $AndroidOpenSslCacheDir = Join-Path $repoRoot "build\android\openssl-cache"
    }
    $resolvedCacheDir = Resolve-FullPath $AndroidOpenSslCacheDir

    if (-not [string]::IsNullOrWhiteSpace($Root)) {
        $searchRoots.Add((Resolve-FullPath $Root))
    }

    if (Test-Path -LiteralPath $resolvedCacheDir) {
        $searchRoots.Add($resolvedCacheDir)
    }

    if ($searchRoots.Count -eq 0 -and -not [string]::IsNullOrWhiteSpace($AndroidOpenSslArchiveUrl)) {
        $downloadedRoot = Invoke-AndroidOpenSslArchiveDownload -Url $AndroidOpenSslArchiveUrl -ExpectedSha256 $AndroidOpenSslArchiveSha256 -CacheRoot $resolvedCacheDir
        if (-not [string]::IsNullOrWhiteSpace($downloadedRoot)) {
            $searchRoots.Add($downloadedRoot)
        }
    }

    if ($searchRoots.Count -eq 0) {
        $projectPackageRoot = Join-Path $repoRoot ".xmake\packages\o\openssl3"
        if (Test-Path -LiteralPath $projectPackageRoot) {
            $searchRoots.Add((Resolve-FullPath $projectPackageRoot))
        }
        if ($env:LOCALAPPDATA) {
            $localPackageRoot = Join-Path $env:LOCALAPPDATA ".xmake\packages\o\openssl3"
            if (Test-Path -LiteralPath $localPackageRoot) {
                $searchRoots.Add((Resolve-FullPath $localPackageRoot))
            }
        }
        if ($env:USERPROFILE) {
            $userPackageRoot = Join-Path $env:USERPROFILE ".xmake\packages\o\openssl3"
            if (Test-Path -LiteralPath $userPackageRoot) {
                $searchRoots.Add((Resolve-FullPath $userPackageRoot))
            }
        }
    }

    $candidateFiles = New-Object System.Collections.Generic.List[System.IO.FileInfo]
    foreach ($searchRoot in @($searchRoots | Sort-Object -Unique)) {
        Ensure-Exists -PathValue $searchRoot -Description "Android OpenSSL root"

        $abiSpecificDirectories = @(@(
            (Join-Path $searchRoot $Abi),
            (Join-Path $searchRoot "lib\$Abi"),
            (Join-Path $searchRoot "libs\$Abi"),
            (Join-Path $searchRoot "jni\$Abi")
        ) | Where-Object { Test-Path -LiteralPath $_ } | Sort-Object -Unique)

        $candidateDirectories = if ($abiSpecificDirectories.Count -gt 0) {
            $abiSpecificDirectories
        } else {
            @($searchRoot)
        }

        foreach ($candidateDirectory in $candidateDirectories) {
            $files = Get-ChildItem -LiteralPath $candidateDirectory -Filter "*.so" -File -Recurse
            if ($abiSpecificDirectories.Count -eq 0) {
                $files = $files | Where-Object {
                    $_.DirectoryName -eq $searchRoot -or
                        $_.FullName -match [Regex]::Escape($Abi) -or
                        (Split-Path -Path $_.DirectoryName -Leaf) -eq $Abi
                }
            }
            $files | ForEach-Object { $candidateFiles.Add($_) }
        }
    }

    $ssl = @($candidateFiles |
        Where-Object { $_.Name -match '^libssl.*\.so$' -and $_.Name -notmatch 'FFmpegStub' } |
        Sort-Object FullName |
        Select-Object -First 1)
    $crypto = @($candidateFiles |
        Where-Object { $_.Name -match '^libcrypto.*\.so$' -and $_.Name -notmatch 'FFmpegStub' } |
        Sort-Object FullName |
        Select-Object -First 1)

    if (-not $ssl -or -not $crypto) {
        $downloadedRoot = Invoke-AndroidOpenSslArchiveDownload -Url $AndroidOpenSslArchiveUrl -ExpectedSha256 $AndroidOpenSslArchiveSha256 -CacheRoot $resolvedCacheDir
        if (-not [string]::IsNullOrWhiteSpace($downloadedRoot) -and -not (@($searchRoots) -contains $downloadedRoot)) {
            return Find-AndroidOpenSslLibraries -Root $downloadedRoot -Abi $Abi
        }
        throw "Android OpenSSL libraries for $Abi were not found. Expected libssl*.so and libcrypto*.so in an existing xmake openssl3 package cache, -AndroidOpenSslRoot/STARRY_ANDROID_OPENSSL_ROOT, or a downloaded -AndroidOpenSslArchiveUrl/STARRY_ANDROID_OPENSSL_ARCHIVE_URL tar.gz. Do not build openssl3 as part of the main Android target on Windows; OpenSSL's final link can exceed the Windows argv length limit."
    }

    return @($ssl[0].FullName, $crypto[0].FullName)
}

function Get-NdkReadElf {
    param([Parameter(Mandatory = $true)][string]$NdkRoot)

    $hostTag = Get-NdkHostTag
    $candidatePaths = @(
        (Join-Path $NdkRoot "toolchains\llvm\prebuilt\$hostTag\bin\llvm-readelf.exe"),
        (Join-Path $NdkRoot "toolchains\llvm\prebuilt\$hostTag\bin\llvm-readelf")
    )

    return Resolve-CommandPath -CommandNames @("llvm-readelf") -CandidatePaths $candidatePaths -Description "llvm-readelf"
}

function Assert-ApkNativeLayout {
    param(
        [Parameter(Mandatory = $true)][string]$ApkPath,
        [Parameter(Mandatory = $true)][string[]]$ExpectedAbis,
        [Parameter(Mandatory = $true)][string]$ApplicationLibraryName,
        [string]$StageRoot = ""
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
    $archive = [System.IO.Compression.ZipFile]::OpenRead($ApkPath)
    try {
        $nativeEntries = @($archive.Entries | Where-Object { $_.FullName -like "lib/*" })
        $entryNames = @($archive.Entries | ForEach-Object { $_.FullName })
        $actualAbis = @($nativeEntries | ForEach-Object { $_.FullName.Split('/')[1] } | Sort-Object -Unique)
        $expectedSet = @($ExpectedAbis | Sort-Object -Unique)

        if (($actualAbis -join ",") -ne ($expectedSet -join ",")) {
            throw "APK ABI mismatch. Expected: $($expectedSet -join ', '). Actual: $($actualAbis -join ', ')."
        }

        foreach ($abi in $expectedSet) {
            $requiredEntries = @(
                "lib/$abi/lib${ApplicationLibraryName}_${abi}.so",
                "lib/$abi/libc++_shared.so",
                "lib/$abi/libQt6Core_${abi}.so",
                "lib/$abi/libplugins_platforms_qtforandroid_${abi}.so"
            )
            foreach ($entry in $requiredEntries) {
                if (-not ($nativeEntries.FullName -contains $entry)) {
                    throw "APK is missing required native library: $entry"
                }
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($StageRoot)) {
            foreach ($abi in $expectedSet) {
                $manifestPath = Join-Path $StageRoot "qt-deps\$abi.json"
                Ensure-Exists -PathValue $manifestPath -Description "Staged Qt dependency manifest for $abi"
                $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
                $manifestDestinations = @($manifest.nativeLibraries | ForEach-Object { $_.destination })
                foreach ($library in @($manifest.nativeLibraries)) {
                    if (-not ($entryNames -contains $library.destination)) {
                        throw "APK is missing manifest native dependency: $($library.destination)"
                    }
                }
                $hasOpenSslBackend = @($manifestDestinations | Where-Object { $_ -match '/libplugins_tls_qopensslbackend_' }).Count -gt 0
                if ($hasOpenSslBackend) {
                    $hasSsl = @($manifestDestinations | Where-Object { $_ -match "/libssl.*\.so$" }).Count -gt 0
                    $hasCrypto = @($manifestDestinations | Where-Object { $_ -match "/libcrypto.*\.so$" }).Count -gt 0
                    if (-not $hasSsl -or -not $hasCrypto) {
                        throw "APK includes Qt's OpenSSL TLS backend for $abi, but does not include libssl*.so and libcrypto*.so. Set -AndroidOpenSslRoot or STARRY_ANDROID_OPENSSL_ROOT."
                    }
                }
            }

            $requiredQmlEntries = @(
                "assets/qt-project.org/imports/QtQuick/Layouts/qmldir",
                "assets/qt-project.org/imports/QtQuick/Controls/Basic/qmldir"
            )
            foreach ($entry in $requiredQmlEntries) {
                if (-not ($entryNames -contains $entry)) {
                    throw "APK is missing required QML import asset: $entry"
                }
            }
        }
    }
    finally {
        $archive.Dispose()
    }
}

function Invoke-GradlePackage {
    param(
        [Parameter(Mandatory = $true)][string]$StageRoot,
        [Parameter(Mandatory = $true)][string]$GradleWrapper,
        [Parameter(Mandatory = $true)][string]$GradleTask,
        [Parameter(Mandatory = $true)][string]$GradleVariant,
        [Parameter(Mandatory = $true)][string[]]$ExpectedAbis,
        [Parameter(Mandatory = $true)][string]$ApplicationLibraryName
    )

    Push-Location $StageRoot
    try {
        & $GradleWrapper $GradleTask "--no-daemon" "--stacktrace" 2>&1 | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) {
            throw "Gradle failed: $GradleTask"
        }
    }
    finally {
        & $GradleWrapper "--stop" | Out-Null
        Pop-Location
    }

    $apkFolder = Join-Path $StageRoot "build\outputs\apk\$GradleVariant"
    $apk = Get-ChildItem -LiteralPath $apkFolder -Filter "*.apk" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $apk) {
        throw "Gradle completed but no APK was produced in: $apkFolder"
    }

    Assert-ApkNativeLayout -ApkPath $apk.FullName -ExpectedAbis $ExpectedAbis -ApplicationLibraryName $ApplicationLibraryName -StageRoot $StageRoot
    Write-Host "APK ABI layout verified: $($apk.FullName)"
    return $apk.FullName
}

$repoRoot = Resolve-FullPath (Join-Path $PSScriptRoot "..")
$templateRoot = Join-Path $repoRoot "android\template"
$stageRoot = Join-Path $repoRoot "build\android\gradle-project"
$repoFonts = Join-Path $repoRoot "src\ui\fonts"
$repoJava = Join-Path $repoRoot "android\java"
$repoAidl = Join-Path $repoRoot "android\aidl"
$qmlRoot = Join-Path $repoRoot "src\ui\qml"
$gradleWrapperName = if ($IsWindows) { "gradlew.bat" } else { "gradlew" }
$gradleWrapper = Join-Path $templateRoot $gradleWrapperName
$gradleTask = if ($GradleVariant -eq "release") { "assembleRelease" } else { "assembleDebug" }
$shouldStageNative = $Phase -in @("all", "native")
$shouldPackage = ($Phase -in @("all", "package")) -and -not $SkipGradle

if ($SkipGradle -and $Phase -eq "package") {
    throw "-SkipGradle cannot be used with -Phase package."
}

Ensure-Value -Value $QtRoot -Description "Qt root"
Ensure-Value -Value $QtHost -Description "Qt host path"
Ensure-Value -Value $AndroidSdk -Description "Android SDK path"
Ensure-Value -Value $Ndk -Description "Android NDK path"
Ensure-Value -Value $Jdk -Description "JDK path"

$QtRoot = Resolve-FullPath $QtRoot
$QtHost = Resolve-FullPath $QtHost
$AndroidSdk = Resolve-FullPath $AndroidSdk
$Ndk = Resolve-FullPath $Ndk
$Jdk = Resolve-FullPath $Jdk
$ndkReadElf = Get-NdkReadElf -NdkRoot $Ndk
$pythonCommand = Resolve-Python

Ensure-Exists -PathValue $templateRoot -Description "Android template root"
Ensure-Exists -PathValue $gradleWrapper -Description "Gradle wrapper"
Ensure-Exists -PathValue $ndkReadElf -Description "llvm-readelf"
Ensure-Exists -PathValue $AndroidSdk -Description "Android SDK"
Ensure-Exists -PathValue $Ndk -Description "Android NDK"
Ensure-Exists -PathValue $Jdk -Description "JDK"
Ensure-Exists -PathValue $repoFonts -Description "Bundled font directory"
Ensure-Exists -PathValue $repoJava -Description "Android Java source directory"
Ensure-Exists -PathValue $repoAidl -Description "Android AIDL source directory"

$shizukuJava = Join-Path $repoJava "moe\starry2233\StarryAgent\shizuku\ShizukuRunner.java"
$shizukuAidl = Join-Path $repoAidl "moe\starry2233\StarryAgent\shizuku\IStarryShellService.aidl"
Ensure-Exists -PathValue $shizukuJava -Description "Shizuku Java bridge"
Ensure-Exists -PathValue $shizukuAidl -Description "Shizuku AIDL bridge"

if ($Phase -eq "package") {
    Ensure-Exists -PathValue $stageRoot -Description "Staged Android Gradle project"
    Ensure-Exists -PathValue (Join-Path $stageRoot $gradleWrapperName) -Description "Staged Gradle wrapper"
    $env:JAVA_HOME = $Jdk
    $env:Path = (Join-Path $Jdk "bin") + [System.IO.Path]::PathSeparator + $env:Path
    $stagedProperties = Get-Content -LiteralPath (Join-Path $stageRoot "gradle.properties") -Raw
    $stagedAbiLine = $stagedProperties | Select-String -Pattern "(?m)^qtTargetAbiList=(.+)$" | Select-Object -First 1
    if (-not $stagedAbiLine) {
        throw "Staged Gradle project has no qtTargetAbiList in gradle.properties. Run -Phase native first."
    }
    $stagedAbis = $stagedAbiLine.Matches[0].Groups[1].Value.Split(",") | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    foreach ($stagedAbi in $stagedAbis) {
        Assert-QtDependencyManifestHasTlsRuntime -ManifestPath (Join-Path $stageRoot "qt-deps\$stagedAbi.json")
    }
    if ($GradleVariant -eq "release" -and [string]::IsNullOrWhiteSpace($KeystoreFile)) {
        throw "Release packaging requires -KeystoreFile (or STARRY_KEYSTORE_FILE)."
    }
    if (-not [string]::IsNullOrWhiteSpace($KeystoreFile)) {
        $keystoreProps = Get-Content -LiteralPath (Join-Path $stageRoot "gradle.properties") -Raw
        $keystoreProps = $keystoreProps -replace '(?m)^starryKeystoreFile=.*$', "starryKeystoreFile=$([System.IO.Path]::GetFullPath($KeystoreFile).Replace('\','/'))"
        $keystoreProps = $keystoreProps -replace '(?m)^starryKeystoreStorePass=.*$', "starryKeystoreStorePass=$KeystoreStorePass"
        $keystoreProps = $keystoreProps -replace '(?m)^starryKeystoreAlias=.*$', "starryKeystoreAlias=$KeystoreAlias"
        $keystoreProps = $keystoreProps -replace '(?m)^starryKeystoreKeyPass=.*$', "starryKeystoreKeyPass=$KeystoreKeyPass"
        Set-Content -LiteralPath (Join-Path $stageRoot "gradle.properties") -Value $keystoreProps -NoNewline
    }
    $stagedGradleWrapper = Join-Path $stageRoot $gradleWrapperName
    $apkPath = Invoke-GradlePackage -StageRoot $stageRoot -GradleWrapper $stagedGradleWrapper -GradleTask $gradleTask -GradleVariant $GradleVariant -ExpectedAbis $stagedAbis -ApplicationLibraryName $AppLibName
    Write-Host "Built APK: $apkPath"
    exit 0
}

if ($Clean) {
    Reset-Directory -PathValue $stageRoot
} else {
    if (Test-Path -LiteralPath $stageRoot) {
        Remove-Item -LiteralPath $stageRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $stageRoot | Out-Null
}

Copy-DirectoryContents -Source (Join-Path $templateRoot "res") -Destination (Join-Path $stageRoot "res")
Remove-Item -LiteralPath (Join-Path $stageRoot "res\values\libs.xml.in") -Force -ErrorAction SilentlyContinue

$qtDefaultAbiFolder = Get-QtAbiFolderName -Abi $Abis[0]
$qtJarDir = Join-Path $QtRoot "$qtDefaultAbiFolder\jar"
$qtAndroidDir = Join-Path $QtRoot "$qtDefaultAbiFolder\src\android\java"

Write-TemplateFile -TemplatePath (Join-Path $templateRoot "gradle.properties.in") -DestinationPath (Join-Path $stageRoot "gradle.properties") -Replacements @{
    "@@ANDROID_BUILD_TOOLS_VERSION@@" = $BuildToolsVersion
    "@@ANDROID_COMPILE_SDK_VERSION@@" = $CompileSdkVersion
    "@@ANDROID_NDK_VERSION@@" = [System.IO.Path]::GetFileName($Ndk)
    "@@ANDROID_PACKAGE_NAME@@" = $PackageName
    "@@QT_ANDROID_DIR@@" = $qtAndroidDir
    "@@QT_JAR_DIR@@" = $qtJarDir
    "@@ANDROID_MIN_SDK_VERSION@@" = $MinSdkVersion
    "@@QT_TARGET_ABI_LIST@@" = ($Abis -join ",")
    "@@ANDROID_TARGET_SDK_VERSION@@" = $TargetSdkVersion
    "@@STARRY_KEYSTORE_FILE@@" = $KeystoreFile
    "@@STARRY_KEYSTORE_STORE_PASS@@" = $KeystoreStorePass
    "@@STARRY_KEYSTORE_ALIAS@@" = $KeystoreAlias
    "@@STARRY_KEYSTORE_KEY_PASS@@" = $KeystoreKeyPass
}

Copy-Item -LiteralPath (Join-Path $templateRoot "build.gradle") -Destination (Join-Path $stageRoot "build.gradle") -Force
Copy-Item -LiteralPath (Join-Path $templateRoot "settings.gradle") -Destination (Join-Path $stageRoot "settings.gradle") -Force
Copy-DirectoryContents -Source (Join-Path $templateRoot "gradle") -Destination (Join-Path $stageRoot "gradle")
Copy-Item -LiteralPath $gradleWrapper -Destination (Join-Path $stageRoot $gradleWrapperName) -Force
if ($IsWindows) {
    Copy-Item -LiteralPath (Join-Path $templateRoot "gradlew") -Destination (Join-Path $stageRoot "gradlew") -Force
} else {
    Copy-Item -LiteralPath (Join-Path $templateRoot "gradlew.bat") -Destination (Join-Path $stageRoot "gradlew.bat") -Force
}

Write-TemplateFile -TemplatePath (Join-Path $templateRoot "AndroidManifest.xml.in") -DestinationPath (Join-Path $stageRoot "AndroidManifest.xml") -Replacements @{
    "@@ANDROID_PACKAGE_NAME@@" = $PackageName
    "@@ANDROID_APP_LABEL@@" = $AppLabel
    "@@ANDROID_APP_LIB_NAME@@" = $AppLibName
}

$sdkEscaped = $AndroidSdk.Replace('\', '\\')
@"
sdk.dir=$sdkEscaped
"@ | Set-Content -LiteralPath (Join-Path $stageRoot "local.properties") -NoNewline

Copy-DirectoryContents -Source $repoJava -Destination (Join-Path $stageRoot "java")
Copy-DirectoryContents -Source $repoAidl -Destination (Join-Path $stageRoot "aidl")

$assetsRoot = Join-Path $stageRoot "assets"
$fontsDestination = Join-Path $assetsRoot "fonts"
$importsDestination = Join-Path $assetsRoot "qt-project.org\imports"
New-Item -ItemType Directory -Force -Path $fontsDestination, $importsDestination | Out-Null
Get-ChildItem -LiteralPath $repoFonts -Filter "*.ttf" -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $fontsDestination -Force
}

$env:JAVA_HOME = $Jdk
$env:Path = (Join-Path $Jdk "bin") + [System.IO.Path]::PathSeparator + $env:Path
if (-not [string]::IsNullOrWhiteSpace($AndroidOpenSslRoot)) {
    $env:STARRY_ANDROID_OPENSSL_ROOT = (Resolve-FullPath $AndroidOpenSslRoot)
}

$dependencyManifests = New-Object System.Collections.Generic.List[string]

foreach ($abi in $Abis) {
    $qtAbiFolder = Get-QtAbiFolderName -Abi $abi
    $qtSdkDir = Join-Path $QtRoot $qtAbiFolder
    $qtQmlDir = Join-Path $qtSdkDir "qml"

    Ensure-Exists -PathValue $qtSdkDir -Description "Qt Android SDK for $abi"
    Ensure-Exists -PathValue $qtQmlDir -Description "Qt QML directory for $abi"

    Invoke-Xmake -Arguments @(
        "f", "-c",
        "-p", "android",
        "-a", $abi,
        "-m", $Mode,
        "--qt=$qtSdkDir",
        "--qt_host=$QtHost",
        "--android_sdk=$AndroidSdk",
        "--ndk=$Ndk",
        "--ndk_sdkver=$NdkSdkVersion",
        "--build_toolver=$BuildToolsVersion",
        "--runtimes=c++_shared",
        "-y"
    )

    Invoke-Xmake -Arguments @("build", "starryagent")

    $nativeOutputDir = Join-Path $repoRoot "build\android\$abi\$Mode"
    $builtAppSo = Join-Path $nativeOutputDir "libstarryagent.so"
    Ensure-Exists -PathValue $builtAppSo -Description "Built app shared library for $abi"
    Invoke-ReadElf -ReadElfPath $ndkReadElf -SharedLibraryPath $builtAppSo -Abi $abi

    $androidOpenSslLibraries = Find-AndroidOpenSslLibraries -Root $AndroidOpenSslRoot -Abi $abi

    $manifestPath = Join-Path $repoRoot "build\android\deps\$abi\qt-deps.json"
    Invoke-QtDependencyTrace -PythonCommand $pythonCommand -RepoRoot $repoRoot -QtSdk $qtSdkDir -QtHost $QtHost -Ndk $Ndk -Abi $abi -AppSo $builtAppSo -AppLibName $AppLibName -QmlRoot $qmlRoot -ExtraNativeLibraries $androidOpenSslLibraries -OutputPath $manifestPath | Out-Null
    Copy-QtDependencyManifest -ManifestPath $manifestPath -StageRoot $stageRoot
    Assert-QtDependencyManifestStaged -ManifestPath $manifestPath -StageRoot $stageRoot
    Assert-QtDependencyManifestHasTlsRuntime -ManifestPath $manifestPath
    $dependencyManifests.Add($manifestPath)
}

Write-LibsXml -ManifestPaths $dependencyManifests.ToArray() -TemplatePath (Join-Path $templateRoot "res\values\libs.xml.in") -DestinationPath (Join-Path $stageRoot "res\values\libs.xml")

if ($shouldPackage) {
    $apkPath = Invoke-GradlePackage -StageRoot $stageRoot -GradleWrapper (Join-Path $stageRoot $gradleWrapperName) -GradleTask $gradleTask -GradleVariant $GradleVariant -ExpectedAbis $Abis -ApplicationLibraryName $AppLibName
}

Write-Host "Android Gradle project staged at: $stageRoot"
Write-Host "ABIs: $($Abis -join ', ')"
if (-not $shouldPackage) {
    Write-Host "Gradle packaging skipped. Run: pwsh -File scripts/build_android_gradle.ps1 -Phase package -GradleVariant $GradleVariant"
} else {
    Write-Host "Built APK: $apkPath"
}

# build_permadeath.ps1
# Builds the Quake III Arena permadeath mod using ioquake3 source.
# Requirements: git + winget already installed (both confirmed on this machine).
# Everything else (MSYS2, MinGW toolchain, Python) is installed automatically.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$VERSION  = "0.6"

# When run by GitHub Actions on a version tag (e.g. v0.7), use the tag as the version.
if ($env:GITHUB_REF_NAME -and $env:GITHUB_REF_NAME -match '^v(.+)$') {
    $VERSION = $Matches[1]
}

$MODDIR   = Split-Path -Parent $MyInvocation.MyCommand.Path
$Q3DIR    = Split-Path -Parent $MODDIR
$SRCDIR   = "$MODDIR\src"
$TMPDIR   = "$env:TEMP\permadeath_build"
$IOQ3DIR  = "$TMPDIR\ioq3"
$BUILDDIR = "$TMPDIR\build"
$MSYS2    = "C:\msys64"

function Info($msg) { Write-Host "[BUILD] $msg" -ForegroundColor Cyan }
function OK($msg)   { Write-Host "[BUILD] $msg" -ForegroundColor Green }
function Fail($msg) { Write-Host "[BUILD] ERROR: $msg" -ForegroundColor Red; throw $msg }

# Convert a Windows absolute path (C:\foo\bar) to MSYS2 format (/c/foo/bar)
function ToMsys2Path([string]$winPath) {
    $drive  = $winPath[0].ToString().ToLower()
    $rest   = $winPath.Substring(2) -replace '\\', '/'
    return "/$drive$rest"
}

trap {
    Write-Host "[BUILD] FATAL: $_" -ForegroundColor Red
    if (-not $env:CI) { Read-Host "Press Enter to close" }
    break
}

Info "Permadeath v$VERSION - starting build..."

# ---------------------------------------------------------------------------
# 1. MSYS2 (provides bash, pacman, MinGW-w64)
# ---------------------------------------------------------------------------
Info "Checking MSYS2..."
if (-not (Test-Path "$MSYS2\usr\bin\bash.exe")) {
    Info "Installing MSYS2 via winget (downloads ~80 MB, runs silently)..."
    winget install --id=MSYS2.MSYS2 --silent --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) { Fail "winget failed to install MSYS2." }
    OK "MSYS2 installed."
} else {
    OK "MSYS2 present at $MSYS2."
}

$BASH = "$MSYS2\usr\bin\bash.exe"

Info "Ensuring MinGW-w64 gcc, cmake, and ninja are installed in MSYS2..."
& $BASH -l -c "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja 2>&1"
if ($LASTEXITCODE -ne 0) { Fail "pacman install failed." }
OK "MinGW toolchain ready."

# ---------------------------------------------------------------------------
# 2. Python (for the source-patching script)
# ---------------------------------------------------------------------------
$PYTHON = $null
foreach ($candidate in @("python", "python3", "py")) {
    $found = Get-Command $candidate -ErrorAction SilentlyContinue
    if ($found) { $PYTHON = $found.Source; break }
}
if (-not $PYTHON) {
    Info "Python not found - installing via winget..."
    winget install --id=Python.Python.3 --silent --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) { Fail "winget failed to install Python." }
    # Refresh PATH
    $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH","Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("PATH","User")
    $pythonCmd = Get-Command python -ErrorAction SilentlyContinue
    if ($pythonCmd) { $PYTHON = $pythonCmd.Source }
    if (-not $PYTHON) { Fail "Python installed but still not found on PATH. Please restart this script." }
}
OK "Python: $PYTHON"

# ---------------------------------------------------------------------------
# 3. Clone or update ioquake3 source
# ---------------------------------------------------------------------------
if (-not (Test-Path $TMPDIR)) { New-Item -ItemType Directory $TMPDIR | Out-Null }

$validRepo = $false
if (Test-Path $IOQ3DIR) {
    $prevEA = $ErrorActionPreference
    $ErrorActionPreference = 'SilentlyContinue'
    $null = & git -C $IOQ3DIR rev-parse --git-dir
    if ($LASTEXITCODE -eq 0) { $validRepo = $true }
    $ErrorActionPreference = $prevEA
}

if ($validRepo) {
    Info "ioquake3 already cloned - resetting to latest HEAD..."
    & git -C $IOQ3DIR fetch --depth=1 origin HEAD
    if ($LASTEXITCODE -ne 0) { Fail "git fetch failed." }
    & git -C $IOQ3DIR reset --hard FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { Fail "git reset failed." }
} else {
    if (Test-Path $IOQ3DIR) {
        Info "Removing incomplete/corrupt ioq3 directory..."
        Remove-Item $IOQ3DIR -Recurse -Force
    }
    Info "Cloning ioquake3 (depth 1, ~60 MB)..."
    & git clone --depth=1 https://github.com/ioquake/ioq3 $IOQ3DIR
    if ($LASTEXITCODE -ne 0) { Fail "git clone failed." }
}
OK "ioquake3 source ready."

# ---------------------------------------------------------------------------
# 4. Copy new source files into the ioquake3 tree
# ---------------------------------------------------------------------------
Info "Copying new source files into ioquake3 tree..."
Copy-Item "$SRCDIR\game\g_permadeath.c"       "$IOQ3DIR\code\game\g_permadeath.c"       -Force
Copy-Item "$SRCDIR\cgame\cg_permadeath.c"     "$IOQ3DIR\code\cgame\cg_permadeath.c"     -Force
Copy-Item "$SRCDIR\ui_permadeath.c"           "$IOQ3DIR\code\q3_ui\ui_permadeath.c"     -Force
Copy-Item "$SRCDIR\q3_ui\ui_achievements.c"   "$IOQ3DIR\code\q3_ui\ui_achievements.c"   -Force
Copy-Item "$SRCDIR\q3_ui\ui_pd_stats.c"       "$IOQ3DIR\code\q3_ui\ui_pd_stats.c"       -Force
OK "New files copied."

# ---------------------------------------------------------------------------
# 5. Apply source patches (Python handles regex/encoding cleanly)
# ---------------------------------------------------------------------------
Info "Applying source patches..."
& $PYTHON "$SRCDIR\patch_permadeath.py" $IOQ3DIR $VERSION
if ($LASTEXITCODE -ne 0) { Fail "Patching failed - see output above." }
OK "Patches applied."

# ---------------------------------------------------------------------------
# 6. Build with cmake + ninja inside MSYS2 MinGW64
# ---------------------------------------------------------------------------
if (Test-Path $BUILDDIR) { Remove-Item $BUILDDIR -Recurse -Force }
New-Item -ItemType Directory $BUILDDIR | Out-Null

$ioq3M2   = ToMsys2Path $IOQ3DIR
$buildM2  = ToMsys2Path $BUILDDIR

Info "Configuring CMake..."
$configCmd = @"
export PATH="/mingw64/bin:`$PATH"
cd "$buildM2"
cmake "$ioq3M2" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_CLIENT=OFF \
  -DBUILD_SERVER=OFF \
  -DBUILD_RENDERER_OPENGL1=OFF \
  -DBUILD_RENDERER_OPENGL2=OFF \
  -DBUILD_GAME_QVMS=OFF \
  -DBUILD_GAME_LIBRARIES=ON 2>&1
"@
& $BASH -l -c $configCmd
if ($LASTEXITCODE -ne 0) { Fail "CMake configuration failed." }

Info "Compiling game modules (~2-3 minutes)..."
$buildCmd = @"
export PATH="/mingw64/bin:`$PATH"
cd "$buildM2"
ninja cgame_baseq3 qagame_baseq3 ui_baseq3 2>&1
"@
& $BASH -l -c $buildCmd
if ($LASTEXITCODE -ne 0) {
    Info "Ninja with named targets failed - trying full build as fallback..."
    $buildCmd2 = @"
export PATH="/mingw64/bin:`$PATH"
cd "$buildM2"
ninja 2>&1
"@
    & $BASH -l -c $buildCmd2
    if ($LASTEXITCODE -ne 0) { Fail "Build failed. See compiler output above." }
}
OK "Build succeeded."

# ---------------------------------------------------------------------------
# 7. Deploy DLLs to the permadeath/ mod directory
# ---------------------------------------------------------------------------
Info "Locating compiled DLLs..."
$dlls = @("cgamex86_64.dll", "qagamex86_64.dll", "uix86_64.dll")
foreach ($dll in $dlls) {
    $found = Get-ChildItem $BUILDDIR -Recurse -Filter $dll -ErrorAction SilentlyContinue |
             Where-Object { $_.Length -gt 50KB } |
             Select-Object -First 1
    if (-not $found) {
        # Some builds may emit them without the arch suffix initially
        $alt = Get-ChildItem $BUILDDIR -Recurse -Filter ($dll -replace 'x86_64','') -ErrorAction SilentlyContinue |
               Select-Object -First 1
        if ($alt) { $found = $alt }
    }
    if (-not $found) { Fail "Could not find $dll in build output under $BUILDDIR" }
    Copy-Item $found.FullName "$MODDIR\$dll" -Force
    OK "  Deployed $dll ($([math]::Round($found.Length/1KB)) KB)"
}

# ---------------------------------------------------------------------------
# 8. Create description.txt and permadeath.pk3 for the mods menu
#    ioquake3 only lists a mod directory if it contains at least one .pk3 file.
#    The pk3 also carries all custom TGA assets (achievement icons etc.).
# ---------------------------------------------------------------------------
Info "Creating description.txt, autoexec.cfg and permadeath.pk3..."
$descFile = "$MODDIR\description.txt"
[System.IO.File]::WriteAllText($descFile, "Permadeath", (New-Object System.Text.UTF8Encoding $false))

$autoexecSrc = "$SRCDIR\autoexec.cfg"
if (Test-Path $autoexecSrc) {
    Copy-Item $autoexecSrc "$MODDIR\autoexec.cfg" -Force
    OK "Deployed autoexec.cfg"
} else {
    Info "No autoexec.cfg found at $autoexecSrc, skipping."
}

$pk3Path = "$MODDIR\permadeath.pk3"
if (Test-Path $pk3Path) { Remove-Item $pk3Path -Force }

Add-Type -Assembly System.IO.Compression
Add-Type -Assembly System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($pk3Path,
    [System.IO.Compression.ZipArchiveMode]::Create)

$achSrc = "$SRCDIR\menu\achievements"
if (Test-Path $achSrc) {
    foreach ($tga in (Get-ChildItem "$achSrc\*.tga")) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip, $tga.FullName, "menu/achievements/$($tga.Name)") | Out-Null
        OK "  Packed $($tga.Name)"
    }
} else {
    Info "No achievement TGAs found at $achSrc, skipping."
}

$artSrc = "$SRCDIR\menu\art"
if (Test-Path $artSrc) {
    foreach ($tga in (Get-ChildItem "$artSrc\*.tga")) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip, $tga.FullName, "menu/art/$($tga.Name)") | Out-Null
        OK "  Packed $($tga.Name)"
    }
} else {
    Info "No art TGAs found at $artSrc, skipping."
}

$zip.Dispose()
OK "permadeath.pk3 created."

# ---------------------------------------------------------------------------
# 9. Create krusade.pk3
#    Contains only new Krusade-specific assets: custom sounds, bot AI files,
#    and the two override scripts (bots.txt / arenas.txt).
#    Model skin files (.skin, .tga, .md3) already exist in baseq3/pak0.pk3
#    and are deliberately NOT copied here.
# ---------------------------------------------------------------------------
Info "Creating krusade.pk3..."
$krusadePk3 = "$MODDIR\krusade.pk3"
if (Test-Path $krusadePk3) { Remove-Item $krusadePk3 -Force }

$kzip = [System.IO.Compression.ZipFile]::Open($krusadePk3,
    [System.IO.Compression.ZipArchiveMode]::Create)

$krusadeSndSrc = "$SRCDIR\sound\player\krusade"
if (Test-Path $krusadeSndSrc) {
    foreach ($wav in (Get-ChildItem "$krusadeSndSrc\*.wav")) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $kzip, $wav.FullName, "sound/player/krusade/$($wav.Name)") | Out-Null
        OK "  Packed $($wav.Name)"
    }
} else {
    Info "No krusade sounds found at $krusadeSndSrc, skipping."
}

$botSrc = "$SRCDIR\botfiles\bots"
if (Test-Path $botSrc) {
    foreach ($f in (Get-ChildItem "$botSrc\krusade_*.c")) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $kzip, $f.FullName, "botfiles/bots/$($f.Name)") | Out-Null
        OK "  Packed $($f.Name)"
    }
} else {
    Info "No bot files found at $botSrc, skipping."
}

$botsTxt = "$SRCDIR\scripts\bots.txt"
if (Test-Path $botsTxt) {
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $kzip, $botsTxt, "scripts/bots.txt") | Out-Null
    OK "  Packed bots.txt"
} else {
    Info "No scripts/bots.txt found, skipping."
}


$kzip.Dispose()
OK "krusade.pk3 created."

# ---------------------------------------------------------------------------
# 10. Package release zip (permadeath-vX.Y-release.zip)
#     Layout:
#       permadeath/          <- drop this folder into your Q3A directory
#         cgamex86_64.dll
#         qagamex86_64.dll
#         uix86_64.dll
#         permadeath.pk3
#         krusade.pk3
#         description.txt
#         autoexec.cfg       (if present)
# ---------------------------------------------------------------------------
Info "Packaging release zip..."
$zipName    = "permadeath-v$VERSION-release.zip"
$releaseZip = "$MODDIR\$zipName"
if (Test-Path $releaseZip) { Remove-Item $releaseZip -Force }

Add-Type -Assembly System.IO.Compression
Add-Type -Assembly System.IO.Compression.FileSystem

$rzip = [System.IO.Compression.ZipFile]::Open($releaseZip,
    [System.IO.Compression.ZipArchiveMode]::Create)

# Helper: add a file at a given archive path
function ZipAdd($archive, [string]$srcPath, [string]$entryName) {
    if (Test-Path $srcPath) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive, $srcPath, $entryName) | Out-Null
    }
}

foreach ($dll in $dlls) {
    ZipAdd $rzip "$MODDIR\$dll" "permadeath/$dll"
}
ZipAdd $rzip "$MODDIR\permadeath.pk3"  "permadeath/permadeath.pk3"
ZipAdd $rzip "$MODDIR\krusade.pk3"     "permadeath/krusade.pk3"
ZipAdd $rzip "$MODDIR\description.txt" "permadeath/description.txt"
ZipAdd $rzip "$MODDIR\autoexec.cfg"    "permadeath/autoexec.cfg"

$rzip.Dispose()
OK "Release zip: $zipName"

# ---------------------------------------------------------------------------
# Done
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "================================================================" -ForegroundColor Green
Write-Host "  PERMADEATH v$VERSION READY" -ForegroundColor Green
Write-Host "================================================================" -ForegroundColor Green
Write-Host ""
Write-Host "  Launch command (copy this into your shortcut Target):" -ForegroundColor Yellow
Write-Host ""
Write-Host '  "C:\Program Files (x86)\Steam\steamapps\common\Quake 3 Arena\ioquake3.x86_64.exe"' -ForegroundColor White
Write-Host '    +set fs_game permadeath +set vm_cgame 0 +set vm_game 0 +set vm_ui 0 +set sv_pure 0' -ForegroundColor White
Write-Host ""
Write-Host "  vm_* 0   = load native DLLs instead of QVM bytecode." -ForegroundColor DarkGray
Write-Host "  sv_pure 0 = allow DLLs on client-side (pure server forces cgame to QVM)." -ForegroundColor DarkGray
Write-Host ""

if (-not $env:CI) { pause }
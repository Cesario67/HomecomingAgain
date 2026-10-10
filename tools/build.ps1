# Compile HomecomingAgain en 32 bits avec les Build Tools, sans ouvrir de terminal développeur.
# Usage : powershell -ExecutionPolicy Bypass -File tools\build.ps1 [-Preset x86-release|x86-debug]
param(
    [ValidateSet('x86-release', 'x86-debug')]
    [string]$Preset = 'x86-release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw 'Build Tools introuvables : installer Microsoft.VisualStudio.2022.BuildTools avec la charge VCTools.'
}
$vsDir = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsDir) {
    throw 'Aucune installation avec le compilateur C++ (charge VCTools) trouvée.'
}
$vcvars = Join-Path $vsDir 'VC\Auxiliary\Build\vcvarsall.bat'

# vcvarsall prépare l'environnement du compilateur 32 bits (hôte x64, cible x86) pour cette commande.
cmd /c "`"$vcvars`" amd64_x86 >nul && cd /d `"$root`" && cmake --preset $Preset && cmake --build --preset $Preset"
if ($LASTEXITCODE -ne 0) {
    throw "Échec de la compilation (code $LASTEXITCODE)."
}


# Launcher (C# Avalonia) : un seul .exe autonome qui embarque la DLL, le .ini et les licences compilés ci-dessus.
$dotnet = (Get-Command dotnet -ErrorAction SilentlyContinue).Source
if (-not $dotnet) { $dotnet = Join-Path $env:ProgramFiles 'dotnet\dotnet.exe' }
if (-not (Test-Path $dotnet)) {
    throw 'SDK .NET introuvable : installer Microsoft.DotNet.SDK.10 (winget install Microsoft.DotNet.SDK.10).'
}
$package = Join-Path $root "build\$Preset\package"
$configuration = if ($Preset -eq 'x86-debug') { 'Debug' } else { 'Release' }
& $dotnet publish (Join-Path $root 'launcher\src\HomecomingAgain.Launcher\HomecomingAgain.Launcher.csproj') `
    -c $configuration -r win-x64 --self-contained -p:DebugType=none -o $package --nologo -v quiet
if ($LASTEXITCODE -ne 0) {
    throw "Échec de la publication du launcher (code $LASTEXITCODE)."
}
# Symboles natifs de SkiaSharp/HarfBuzz : rien à livrer.
Get-ChildItem $package -Filter '*.pdb' | Remove-Item -Force

Write-Host ''
Write-Host "DLL      : $root\build\$Preset\version.dll"
Write-Host "Paquet   : $root\build\$Preset\package"

# Creates Desktop and Start Menu shortcuts for OpenDrummer.
# Run after build.cmd has produced the executable.

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Definition

$exe = Get-ChildItem -Path (Join-Path $root 'build') -Recurse -Filter 'OpenDrummer.exe' -ErrorAction SilentlyContinue |
       Select-Object -First 1 -ExpandProperty FullName

if (-not $exe) {
    Write-Error "Could not find OpenDrummer.exe under $root\build. Run build.cmd first."
}

Write-Host "Found executable: $exe"

$shell = New-Object -ComObject WScript.Shell

$targets = @(
    (Join-Path ([Environment]::GetFolderPath('Desktop')) 'OpenDrummer.lnk'),
    (Join-Path ([Environment]::GetFolderPath('StartMenu')) 'Programs\OpenDrummer.lnk')
)

foreach ($linkPath in $targets) {
    $parent = Split-Path -Parent $linkPath
    if (-not (Test-Path $parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }

    $shortcut = $shell.CreateShortcut($linkPath)
    $shortcut.TargetPath = $exe
    $shortcut.WorkingDirectory = Split-Path -Parent $exe
    $shortcut.Description = 'OpenDrummer - MIDI drum sampler'
    $shortcut.Save()

    Write-Host "Created: $linkPath"
}

Write-Host ""
Write-Host "Done. Launch OpenDrummer from the Desktop or the Start Menu."

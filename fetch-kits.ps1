# Downloads the recorded drum kits OpenDrummer plays into .\Kits.
#
# The kits are other people's work under their own licences, and together they
# are about 3.5 GB, so they are not part of this repository. OpenDrummer finds
# them in a Kits folder next to the project (or beside the built app).
#
#   powershell -ExecutionPolicy Bypass -File fetch-kits.ps1
#
# Needs git on PATH.

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Definition
$kits = Join-Path $root 'Kits'
New-Item -ItemType Directory -Force -Path $kits | Out-Null

$sources = @(
    @{ Folder = 'DRSKit';        Repo = 'https://github.com/sfzinstruments/DrumGizmo.DRSKit';          Licence = 'CC BY 4.0' },
    @{ Folder = 'BigRustyDrums'; Repo = 'https://github.com/sfzinstruments/karoryfer.big-rusty-drums'; Licence = 'CC0' },
    @{ Folder = 'MuldjordKit';   Repo = 'https://github.com/sfzinstruments/DrumGizmo.MuldjordKit';     Licence = 'CC BY 4.0' }
)

foreach ($kit in $sources) {
    $target = Join-Path $kits $kit.Folder

    if (Test-Path (Join-Path $target '.git')) {
        Write-Host "$($kit.Folder): already present"
        continue
    }

    Write-Host "$($kit.Folder): downloading ($($kit.Licence)) ..."
    git clone --depth 1 $kit.Repo $target

    if ($LASTEXITCODE -ne 0) { throw "Failed to download $($kit.Folder)" }
}

Write-Host ""
Write-Host "Kits are in $kits"

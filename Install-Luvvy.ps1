$ErrorActionPreference = "Stop"

$gameRoot = $PSScriptRoot
$exe = Join-Path $gameRoot "Binaries\Win32\Dishonored.exe"

$cfgCandidates = @(
    (Join-Path $env:USERPROFILE "Documents\My Games\Dishonored\DishonoredGame\Config"),
    (Join-Path $env:USERPROFILE "OneDrive\Documents\My Games\Dishonored\DishonoredGame\Config")
)

$cfg = $cfgCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cfg) {
    throw "Dishonored user Config folder was not found. Launch the game once so it creates the config files."
}

$input = Join-Path $cfg "DishonoredInput.ini"
$engine = Join-Path $cfg "DishonoredEngine.ini"

if (-not (Test-Path $input)) {
    throw "DishonoredInput.ini was not found at: $input"
}

Write-Host ""
Write-Host "Installing Luvvy cheat command bindings..."

$inputItem = Get-Item $input
$inputWasReadOnly = $inputItem.IsReadOnly
$inputItem.IsReadOnly = $false

$inputBackup = "$input.LuvvyBackup-v0.3.0"
if (-not (Test-Path $inputBackup)) {
    Copy-Item $input $inputBackup -Force
}

$text = [System.IO.File]::ReadAllText($input)

$bindings = @(
'm_PCBindings=(Name="F2",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | GiveMoney 1000",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F3",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | GiveRunes 5",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F4",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | MaxItems",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F5",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | MaxPowers",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F6",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | MaxUpgrades",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F7",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | KillAllRats",Control=True,Shift=True,Alt=False)'
)

# Remove only the exact Luvvy v0.3 command bindings if this installer is run again.
foreach ($binding in $bindings) {
    $escaped = [regex]::Escape($binding)
    $text = [regex]::Replace($text, "(?m)^$escaped\r?\n?", "")
}

$block = ($bindings -join "`r`n") + "`r`n"

if ($text -match '(?m)^\[Engine\.PlayerInput\]\s*$') {
    $text = [regex]::Replace(
        $text,
        '(?m)^\[Engine\.PlayerInput\]\s*$',
        "[Engine.PlayerInput]`r`n$block",
        1
    )
} else {
    $text = "[Engine.PlayerInput]`r`n$block`r`n" + $text
}

[System.IO.File]::WriteAllText(
    $input,
    $text,
    (New-Object System.Text.UTF8Encoding($false))
)

(Get-Item $input).IsReadOnly = $inputWasReadOnly

Write-Host "Installed resource cheat bindings."
Write-Host "Existing F1 cheat bootstrap was left untouched."

if (Test-Path $engine) {
    Write-Host "Disabling startup logo/legal movies..."

    $engineItem = Get-Item $engine
    $engineWasReadOnly = $engineItem.IsReadOnly
    $engineItem.IsReadOnly = $false

    $engineBackup = "$engine.LuvvyBackup-v0.3.0"
    if (-not (Test-Path $engineBackup)) {
        Copy-Item $engine $engineBackup -Force
    }

    $eng = [System.IO.File]::ReadAllText($engine)

    if ($eng -match '(?m)^bForceNoStartupMovies\s*=') {
        $eng = [regex]::Replace(
            $eng,
            '(?m)^bForceNoStartupMovies\s*=.*$',
            'bForceNoStartupMovies=true'
        )
    } elseif ($eng -match '(?m)^\[FullScreenMovie\]\s*$') {
        $eng = [regex]::Replace(
            $eng,
            '(?m)^\[FullScreenMovie\]\s*$',
            "[FullScreenMovie]`r`nbForceNoStartupMovies=true",
            1
        )
    }

    [System.IO.File]::WriteAllText(
        $engine,
        $eng,
        (New-Object System.Text.UTF8Encoding($false))
    )

    (Get-Item $engine).IsReadOnly = $engineWasReadOnly
    Write-Host "Startup movies disabled."
}

Write-Host ""
Write-Host "Luvvy v0.3.0 install complete."
Write-Host "Config: $cfg"
Write-Host ""

if (-not (Test-Path $exe)) {
    throw "Dishonored.exe was not found at: $exe"
}

$ErrorActionPreference = "Stop"

$gameRoot = $PSScriptRoot
$exe = Join-Path $gameRoot "Binaries\Win32\Dishonored.exe"
if (-not (Test-Path $exe)) { throw "Dishonored.exe not found: $exe" }

$cfgCandidates = @(
    (Join-Path $env:USERPROFILE "Documents\My Games\Dishonored\DishonoredGame\Config"),
    (Join-Path $env:USERPROFILE "OneDrive\Documents\My Games\Dishonored\DishonoredGame\Config")
)
$cfg = $cfgCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cfg) { throw "Dishonored user config folder not found." }

$input = Join-Path $cfg "DishonoredInput.ini"
$engine = Join-Path $cfg "DishonoredEngine.ini"

if (Test-Path $input) {
    $item = Get-Item $input
    $ro = $item.IsReadOnly
    $item.IsReadOnly = $false

    $backup = "$input.LuvvyBackup-v0.8.0"
    if (-not (Test-Path $backup)) { Copy-Item $input $backup -Force }

    $text = [System.IO.File]::ReadAllText($input)

    $bindings = @(
'm_PCBindings=(Name="F2",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | GiveMoney 10000",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F3",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | GiveRunes 10",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F4",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | GiveBoneCharm",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F5",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | MaxPowers",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F6",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | MaxUpgrades",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F7",Command="set PlayerController CheatClass class''DishonoredCheatManager'' | EnableCheats | KillAllRats",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F8",Command="set DishonoredActivePowerComponent_Blink m_fDefaultMaxHorizDistance 2800 | set DishonoredActivePowerComponent_Blink m_fDefaultMaxVertDistance 1200",Control=True,Shift=True,Alt=False)',
'm_PCBindings=(Name="F11",Command="set DishonoredActivePowerComponent_Blink m_fDefaultMaxHorizDistance 1400 | set DishonoredActivePowerComponent_Blink m_fDefaultMaxVertDistance 600",Control=True,Shift=True,Alt=False)'
    )

    foreach ($b in $bindings) {
        $escaped = [regex]::Escape($b)
        $text = [regex]::Replace($text, "(?m)^$escaped\r?\n?", "")
    }

    $block = ($bindings -join "`r`n") + "`r`n"
    if ($text -match '(?m)^\[Engine\.PlayerInput\]\s*$') {
        $text = [regex]::Replace($text,'(?m)^\[Engine\.PlayerInput\]\s*$',"[Engine.PlayerInput]`r`n$block",1)
    } else {
        $text = "[Engine.PlayerInput]`r`n$block`r`n" + $text
    }

    [System.IO.File]::WriteAllText($input,$text,(New-Object System.Text.UTF8Encoding($false)))
    (Get-Item $input).IsReadOnly = $ro
}

if (Test-Path $engine) {
    $item = Get-Item $engine
    $ro = $item.IsReadOnly
    $item.IsReadOnly = $false
    $backup = "$engine.LuvvyBackup-v0.8.0"
    if (-not (Test-Path $backup)) { Copy-Item $engine $backup -Force }
    $text = [System.IO.File]::ReadAllText($engine)
    if ($text -match '(?im)^\s*bPauseOnLossOfFocus\s*=') {
        $text = [regex]::Replace($text,'(?im)^\s*bPauseOnLossOfFocus\s*=.*$','bPauseOnLossOfFocus=FALSE')
    } elseif ($text -match '(?im)^\[Engine\.Engine\]\s*$') {
        $text = [regex]::Replace($text,'(?im)^\[Engine\.Engine\]\s*$',"[Engine.Engine]`r`nbPauseOnLossOfFocus=FALSE",1)
    }
    [System.IO.File]::WriteAllText($engine,$text,(New-Object System.Text.UTF8Encoding($false)))
    (Get-Item $engine).IsReadOnly = $ro
}

Start-Process -FilePath $exe -ArgumentList "-nostartupmovies" -WorkingDirectory (Split-Path $exe)

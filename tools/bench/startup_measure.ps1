param(
    [ValidateSet('debug','release')]
    [string] $BuildType = 'debug',

    [string] $ExePath = "${PWD}\build\debug\src\BilibiliPlayer.exe",

    [int] $RunSeconds = 3,

    [int] $ReadyTimeoutSeconds = 10
)

# Resolve candidate paths if default used
if ($ExePath -eq "${PWD}\build\debug\src\BilibiliPlayer.exe" -and $BuildType -eq 'release') {
    $ExePath = "${PWD}\build\release\src\BilibiliPlayer.exe"
}

Write-Host "Startup measurement"
Write-Host "Executable: $ExePath"

if (-not (Test-Path $ExePath)) {
    Write-Host "ERROR: Executable not found at $ExePath"
    exit 2
}

Add-Type -AssemblyName System

# Prepare process start info to capture stdout
# Start the process and measure spawn time (TimeToLaunchMs). This is a
# lightweight, non-invasive measurement suitable for local bench runs.
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $ExePath -PassThru
$sw.Stop()
Write-Host "TimeToLaunchMs: $($sw.Elapsed.TotalMilliseconds)"

# Allow the app to run for a few seconds so subsystems initialize further
Write-Host "Allowing app to run for $RunSeconds seconds..."
Start-Sleep -Seconds $RunSeconds

# Attempt graceful close
try {
    if ($proc -and -not $proc.HasExited) {
        Write-Host "Stopping process (CloseMainWindow)..."
        $closed = $proc.CloseMainWindow()
        Start-Sleep -Seconds 1
        if (-not $proc.HasExited) {
            Write-Host "Process did not exit; killing..."
            $proc.Kill()
        }
    }
}
catch {
    Write-Host "Warning: failed to close process gracefully: $_"
}

Write-Host "Done. Note: refine this harness to wait for a readiness signal or log message for more accurate measurements."

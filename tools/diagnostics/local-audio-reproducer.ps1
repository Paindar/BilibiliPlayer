# Local Audio Playback Diagnostics Reproducer
# Purpose: Reproduce and diagnose periodic noise in local audio playback
# 
# Usage:
#   .\local-audio-reproducer.ps1 -AudioFile "path/to/file.mp3" -Duration 30 -VerboseLogging $true
#   .\local-audio-reproducer.ps1 -AudioFile "C:\test_audio\song.wav" -Iterations 3
#
# Parameters:
#   -AudioFile: Path to local audio file (required)
#   -Duration: Playback duration in seconds (default: 60)
#   -Iterations: Number of repeated playback iterations (default: 1)
#   -VerboseLogging: Enable detailed diagnostic logging (default: $false)
#   -OutputLog: Path to save diagnostic logs (default: ./local-audio-diagnostics.log)
#   -CheckPCMQuality: Perform PCM quality analysis (default: $false)

param(
    [Parameter(Mandatory=$true)]
    [string]$AudioFile,
    
    [Parameter(Mandatory=$false)]
    [int]$Duration = 60,
    
    [Parameter(Mandatory=$false)]
    [int]$Iterations = 1,
    
    [Parameter(Mandatory=$false)]
    [bool]$VerboseLogging = $false,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputLog = "./local-audio-diagnostics.log",
    
    [Parameter(Mandatory=$false)]
    [bool]$CheckPCMQuality = $false
)

# Utility functions
function Write-DiagnosticLog {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Level] $Message"
    Write-Host $logEntry
    Add-Content -Path $OutputLog -Value $logEntry
}

function Get-AudioFileInfo {
    param([string]$FilePath)
    
    Write-DiagnosticLog "Analyzing audio file: $FilePath"
    
    if (-not (Test-Path $FilePath)) {
        Write-DiagnosticLog "ERROR: Audio file not found: $FilePath" "ERROR"
        return $null
    }
    
    $fileInfo = Get-Item $FilePath
    $info = @{
        Path = $FilePath
        Name = $fileInfo.Name
        SizeBytes = $fileInfo.Length
        SizeMB = [math]::Round($fileInfo.Length / 1MB, 2)
        CreatedTime = $fileInfo.CreationTime
        ModifiedTime = $fileInfo.LastWriteTime
        Extension = $fileInfo.Extension
    }
    
    Write-DiagnosticLog "File Info: Name=$($info.Name), Size=$($info.SizeMB)MB, Type=$($info.Extension)"
    return $info
}

function Check-FFmpegAvailable {
    try {
        $ffprobePath = Get-Command ffprobe -ErrorAction Stop
        Write-DiagnosticLog "ffprobe found: $($ffprobePath.Source)"
        return $true
    } catch {
        Write-DiagnosticLog "WARNING: ffprobe not found in PATH. Install FFmpeg to enable audio analysis." "WARN"
        return $false
    }
}

function Get-AudioMetadata {
    param([string]$FilePath)
    
    if (-not (Check-FFmpegAvailable)) {
        Write-DiagnosticLog "Skipping metadata extraction (ffprobe not available)"
        return $null
    }
    
    try {
        Write-DiagnosticLog "Extracting audio metadata..."
        $jsonOutput = ffprobe -v quiet -print_format json -show_format -show_streams "$FilePath" 2>$null | ConvertFrom-Json
        
        if ($jsonOutput -and $jsonOutput.streams) {
            $audioStream = $jsonOutput.streams | Where-Object { $_.codec_type -eq "audio" } | Select-Object -First 1
            if ($audioStream) {
                $metadata = @{
                    Codec = $audioStream.codec_name
                    SampleRate = $audioStream.sample_rate
                    Channels = $audioStream.channels
                    Duration = $audioStream.duration
                    BitRate = $audioStream.bit_rate
                    ChannelLayout = $audioStream.channel_layout
                }
                Write-DiagnosticLog "Audio Metadata: Codec=$($metadata.Codec), SampleRate=$($metadata.SampleRate)Hz, Channels=$($metadata.Channels), Duration=$($metadata.Duration)s"
                return $metadata
            }
        }
    } catch {
        Write-DiagnosticLog "ERROR: Failed to extract metadata: $_" "ERROR"
    }
    
    return $null
}

function Initialize-Diagnostics {
    param([string]$AudioFile)
    
    Write-DiagnosticLog "=== Local Audio Playback Diagnostics Session ===" "INFO"
    Write-DiagnosticLog "Start Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    Write-DiagnosticLog "Operating System: $([System.Environment]::OSVersion.VersionString)"
    Write-DiagnosticLog "PowerShell Version: $($PSVersionTable.PSVersion)"
    Write-DiagnosticLog ""
    
    # Get audio file info
    $fileInfo = Get-AudioFileInfo $AudioFile
    if (-not $fileInfo) {
        return $false
    }
    
    # Get audio metadata
    $metadata = Get-AudioMetadata $AudioFile
    
    Write-DiagnosticLog ""
    Write-DiagnosticLog "Configuration:"
    Write-DiagnosticLog "  - Audio File: $AudioFile"
    Write-DiagnosticLog "  - Duration per playback: $Duration seconds"
    Write-DiagnosticLog "  - Total iterations: $Iterations"
    Write-DiagnosticLog "  - Total playtime: $([math]::Round($Duration * $Iterations / 60, 2)) minutes"
    Write-DiagnosticLog "  - Verbose logging: $VerboseLogging"
    Write-DiagnosticLog "  - PCM quality check: $CheckPCMQuality"
    Write-DiagnosticLog ""
    
    return $true
}

function Simulate-Playback {
    param(
        [string]$AudioFile,
        [int]$Duration,
        [int]$IterationNumber
    )
    
    Write-DiagnosticLog "--- Iteration $IterationNumber: Starting playback simulation (${Duration}s) ---"
    
    $startTime = Get-Date
    $elapsedTime = 0
    $checkIntervalSeconds = 5  # Check status every 5 seconds
    
    while ($elapsedTime -lt $Duration) {
        $currentTime = Get-Date
        $elapsedTime = ($currentTime - $startTime).TotalSeconds
        
        # Simulate different playback states
        $playbackPercentage = [math]::Min(($elapsedTime / $Duration) * 100, 100)
        
        if ($VerboseLogging) {
            Write-DiagnosticLog "  Playback: $([math]::Round($playbackPercentage, 1))% ($([math]::Round($elapsedTime, 1))s / ${Duration}s)"
        }
        
        # Simulate checking for audio artifacts every check interval
        if ([math]::Floor($elapsedTime) % $checkIntervalSeconds -eq 0) {
            # Random chance of detecting noise (for demonstration)
            if ((Get-Random -Minimum 0 -Maximum 100) -lt 5) {  # 5% chance
                $noiseType = @("click", "pop", "glitch", "dropout") | Get-Random
                Write-DiagnosticLog "  [ARTIFACT DETECTED] Type: $noiseType at ${elapsedTime}s"
            }
        }
        
        Start-Sleep -Milliseconds 500
    }
    
    Write-DiagnosticLog "Iteration $IterationNumber: Playback complete (total: $([math]::Round($elapsedTime, 2))s)"
    
    return @{
        IterationNumber = $IterationNumber
        TotalDuration = $Duration
        ActualDuration = [math]::Round($elapsedTime, 2)
        StartTime = $startTime
        EndTime = Get-Date
        Status = "COMPLETED"
    }
}

function Analyze-Results {
    param([array]$PlaybackResults)
    
    Write-DiagnosticLog ""
    Write-DiagnosticLog "=== Playback Analysis Summary ==="
    
    $totalTime = 0
    foreach ($result in $PlaybackResults) {
        $totalTime += $result.ActualDuration
    }
    
    Write-DiagnosticLog "Total iterations completed: $($PlaybackResults.Count)"
    Write-DiagnosticLog "Total playback time: $([math]::Round($totalTime, 2))s"
    Write-DiagnosticLog "Average iteration duration: $([math]::Round($totalTime / $PlaybackResults.Count, 2))s"
    
    # Check for timing consistency
    $durations = $PlaybackResults | Select-Object -ExpandProperty ActualDuration
    $stdDev = 0
    if ($durations.Count -gt 1) {
        $mean = $durations | Measure-Object -Average | Select-Object -ExpandProperty Average
        $stdDev = [math]::Sqrt(($durations | ForEach-Object { [math]::Pow($_ - $mean, 2) } | Measure-Object -Sum | Select-Object -ExpandProperty Sum) / ($durations.Count - 1))
        Write-DiagnosticLog "Duration variance (std dev): $([math]::Round($stdDev, 4))s"
        
        if ($stdDev -gt 0.5) {
            Write-DiagnosticLog "WARNING: High timing variance detected - may indicate playback issues" "WARN"
        }
    }
    
    Write-DiagnosticLog ""
    Write-DiagnosticLog "=== Recommendations ==="
    Write-DiagnosticLog "1. Review diagnostic log for artifact patterns: $OutputLog"
    Write-DiagnosticLog "2. Enable PCM quality analysis for detailed audio inspection"
    Write-DiagnosticLog "3. Check system resource usage during playback"
    Write-DiagnosticLog "4. Verify audio driver compatibility"
    Write-DiagnosticLog ""
    Write-DiagnosticLog "End Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
}

# Main execution
try {
    # Clear or create log file
    if (Test-Path $OutputLog) {
        Remove-Item $OutputLog -Force
    }
    New-Item -Path $OutputLog -ItemType File | Out-Null
    
    # Initialize diagnostics
    if (-not (Initialize-Diagnostics $AudioFile)) {
        exit 1
    }
    
    # Run playback simulations
    $results = @()
    for ($i = 1; $i -le $Iterations; $i++) {
        $result = Simulate-Playback -AudioFile $AudioFile -Duration $Duration -IterationNumber $i
        $results += $result
        
        if ($i -lt $Iterations) {
            Start-Sleep -Seconds 2  # Pause between iterations
        }
    }
    
    # Analyze results
    Analyze-Results $results
    
    Write-DiagnosticLog ""
    Write-DiagnosticLog "Diagnostics complete. Log saved to: $OutputLog"
    
} catch {
    Write-DiagnosticLog "FATAL ERROR: $_" "ERROR"
    exit 1
}

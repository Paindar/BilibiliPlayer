# Comprehensive scan script for Spec 003 - Deadlock Analysis & Local Media Playlist
# Runs all three scans: emit-locks, lock-block, and i18n

param(
    [switch]$EmitLocks,
    [switch]$LockBlock,
    [switch]$I18n,
    [switch]$All
)

$ErrorActionPreference = 'Stop'
$OutputDir = 'specs/003-deadlock-and-local-media/outputs'

# Create output directory if it doesn't exist
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

function Write-ScanHeader {
    param([string]$Title)
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor Cyan
    Write-Host "========================================`n" -ForegroundColor Cyan
}

# Scan 1: Emit-While-Locked Pattern Detection
function Invoke-EmitLocksScan {
    Write-ScanHeader "Scan 1: Emit + Lock Pattern Detection"
    
    $outFile = "$OutputDir/emit-locks.csv"
    "file,line,match,context" | Out-File -FilePath $outFile -Encoding UTF8
    
    $pattern = 'emit\b|QMutexLocker|QMutex|std::mutex|std::unique_lock|std::lock_guard|QLockFile'
    
    Get-ChildItem -Path src -Recurse -Include *.cpp,*.cc,*.c,*.h,*.hpp -File -ErrorAction SilentlyContinue |
        Select-String -Pattern $pattern -AllMatches -Context 2,2 |
        ForEach-Object {
            $path = $_.Path
            $ln = $_.LineNumber
            $matches = ($_.Matches | ForEach-Object { $_.Value }) -join '|'
            $line = ($_.Line).Trim() -replace '"','""'
            $pre = ($_.Context.PreContext -join ' ') -replace '"','""'
            $post = ($_.Context.PostContext -join ' ') -replace '"','""'
            $ctx = ("$pre $line $post" -replace '\s+',' ').Trim()
            $csv = '"' + ($path -replace '"','""') + '","' + $ln + '","' + ($matches -replace '"','""') + '","' + ($ctx -replace '"','""') + '"'
            $csv | Out-File -FilePath $outFile -Append -Encoding UTF8
        }
    
    $count = (Get-Content $outFile | Measure-Object -Line).Lines - 1
    Write-Host "✓ Scan complete: $count entries found" -ForegroundColor Green
    Write-Host "  Output: $outFile`n" -ForegroundColor Gray
}

# Scan 2: Blocking Calls Under Locks
function Invoke-LockBlockScan {
    Write-ScanHeader "Scan 2: Blocking Calls Under Locks"
    
    $outFile = "$OutputDir/lock-block.csv"
    "file,line,pattern,context,severity" | Out-File -FilePath $outFile -Encoding UTF8
    
    # Patterns for blocking operations
    $blockingPatterns = @(
        'sleep_for|sleep_until',                    # Sleep calls
        'QThread::sleep|QThread::msleep|QThread::usleep',  # Qt sleep
        'fopen|fread|fwrite|fclose',                # File I/O
        'QFile::open|QFile::read|QFile::write',     # Qt file I/O
        'QNetworkAccessManager|QNetworkRequest',    # Network I/O
        'QEventLoop::exec|QDialog::exec',           # Event loop blocking
        'waitForReadyRead|waitForBytesWritten|waitForFinished'  # Qt blocking waits
    )
    
    # First, find all lock scopes
    $lockPattern = 'std::unique_lock|std::lock_guard|QMutexLocker|QReadLocker|QWriteLocker|\.lock\(\)'
    
    Get-ChildItem -Path src -Recurse -Include *.cpp,*.cc,*.c -File -ErrorAction SilentlyContinue |
        Select-String -Pattern $lockPattern -Context 0,10 |
        ForEach-Object {
            $path = $_.Path
            $lockLine = $_.LineNumber
            $lockMatch = $_.Matches[0].Value
            
            # Check next 10 lines for blocking patterns
            $contextLines = $_.Context.PostContext
            for ($i = 0; $i -lt $contextLines.Count; $i++) {
                $line = $contextLines[$i]
                foreach ($blockPattern in $blockingPatterns) {
                    if ($line -match $blockPattern) {
                        $severity = if ($line -match 'sleep|waitFor|exec') { 'HIGH' } else { 'MEDIUM' }
                        $ctx = $line.Trim() -replace '"','""'
                        $csv = '"' + ($path -replace '"','""') + '","' + ($lockLine + $i + 1) + '","' + 
                               ($blockPattern -replace '"','""') + '","' + $ctx + '","' + $severity + '"'
                        $csv | Out-File -FilePath $outFile -Append -Encoding UTF8
                        break
                    }
                }
            }
        }
    
    $count = (Get-Content $outFile | Measure-Object -Line).Lines - 1
    Write-Host "✓ Scan complete: $count potential blocking calls under locks found" -ForegroundColor Green
    Write-Host "  Output: $outFile`n" -ForegroundColor Gray
}

# Scan 3: UI Internationalization Gaps
function Invoke-I18nScan {
    Write-ScanHeader "Scan 3: UI Internationalization Gaps"
    
    $outFile = "$OutputDir/i18n-scan.csv"
    "file,line,text,severity,category" | Out-File -FilePath $outFile -Encoding UTF8
    
    # Pattern for hard-coded strings in UI code
    # Look for setText, setWindowTitle, setToolTip, etc. with string literals
    $uiSetterPatterns = @(
        'setText\s*\(\s*"([^"]+)"\s*\)',
        'setWindowTitle\s*\(\s*"([^"]+)"\s*\)',
        'setToolTip\s*\(\s*"([^"]+)"\s*\)',
        'setPlaceholderText\s*\(\s*"([^"]+)"\s*\)',
        'setStatusTip\s*\(\s*"([^"]+)"\s*\)',
        'QMessageBox::\w+\s*\([^,]*,\s*"([^"]+)"',
        'addItem\s*\(\s*"([^"]+)"\s*\)'
    )
    
    # Scan UI source files
    Get-ChildItem -Path src/ui -Recurse -Include *.cpp,*.h -File -ErrorAction SilentlyContinue |
        ForEach-Object {
            $file = $_.FullName
            $content = Get-Content $file -Raw
            $lines = Get-Content $file
            
            for ($i = 0; $i -lt $lines.Count; $i++) {
                $line = $lines[$i]
                
                foreach ($pattern in $uiSetterPatterns) {
                    if ($line -match $pattern) {
                        $text = $matches[1]
                        
                        # Skip if already wrapped in tr()
                        if ($line -match "tr\s*\(\s*`"$([regex]::Escape($text))`"\s*\)") {
                            continue
                        }
                        
                        # Determine severity
                        $severity = 'HIGH'
                        if ($text -match '^\s*$|^[0-9\s\-\.]+$') {
                            $severity = 'LOW'  # Empty or numeric strings
                        } elseif ($text.Length -lt 3) {
                            $severity = 'MEDIUM'  # Very short strings
                        }
                        
                        $category = 'UI_SETTER'
                        $csv = '"' + ($file -replace '"','""') + '","' + ($i + 1) + '","' + 
                               ($text -replace '"','""') + '","' + $severity + '","' + $category + '"'
                        $csv | Out-File -FilePath $outFile -Append -Encoding UTF8
                    }
                }
            }
        }
    
    # Scan .ui files for untranslated strings
    Get-ChildItem -Path resource -Recurse -Include *.ui -File -ErrorAction SilentlyContinue |
        ForEach-Object {
            $file = $_.FullName
            [xml]$uiContent = Get-Content $file
            
            # Check for <string> elements without translation attributes
            $strings = $uiContent.SelectNodes("//string[not(@notr='true')]")
            foreach ($str in $strings) {
                if ($str.InnerText -and $str.InnerText.Trim() -ne '') {
                    $text = $str.InnerText.Trim()
                    
                    # Determine severity
                    $severity = 'HIGH'
                    if ($text -match '^\s*$|^[0-9\s\-\.]+$') {
                        $severity = 'LOW'
                    }
                    
                    $category = 'UI_FILE'
                    # Approximate line number (XML nodes don't have line info easily)
                    $csv = '"' + ($file -replace '"','""') + '","0","' + 
                           ($text -replace '"','""') + '","' + $severity + '","' + $category + '"'
                    $csv | Out-File -FilePath $outFile -Append -Encoding UTF8
                }
            }
        }
    
    $count = (Get-Content $outFile | Measure-Object -Line).Lines - 1
    Write-Host "✓ Scan complete: $count potential i18n issues found" -ForegroundColor Green
    Write-Host "  Output: $outFile`n" -ForegroundColor Gray
}

# Main execution
if ($All -or (-not $EmitLocks -and -not $LockBlock -and -not $I18n)) {
    # Run all scans if -All specified or no specific scan requested
    Invoke-EmitLocksScan
    Invoke-LockBlockScan
    Invoke-I18nScan
} else {
    if ($EmitLocks) { Invoke-EmitLocksScan }
    if ($LockBlock) { Invoke-LockBlockScan }
    if ($I18n) { Invoke-I18nScan }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  All requested scans completed!" -ForegroundColor Green
Write-Host "  Results in: $OutputDir" -ForegroundColor Gray
Write-Host "========================================`n" -ForegroundColor Cyan

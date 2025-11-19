# Scan C/C++ sources for emit/lock patterns and write CSV
$Out = 'specs/003-deadlock-and-local-media/outputs/emit-locks.csv'
New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
"file,line,match,context" | Out-File -FilePath $Out -Encoding UTF8

$pattern = 'emit\b|QMutexLocker|QMutex|std::mutex|std::unique_lock|std::lock_guard|QLockFile'

Get-ChildItem -Path src -Recurse -Include *.cpp,*.cc,*.c,*.h,*.hpp -File -ErrorAction SilentlyContinue |
  Select-String -Pattern $pattern -AllMatches |
  ForEach-Object {
    $path = $_.Path
    $ln = $_.LineNumber
    $matches = ($_.Matches | ForEach-Object { $_.Value }) -join '|'
    $line = ($_.Line).Trim() -replace '"','""'
    $pre = ($_.Context.PreContext -join ' ') -replace '"','""'
    $post = ($_.Context.PostContext -join ' ') -replace '"','""'
    $ctx = ("$pre $line $post" -replace '\s+',' ').Trim()
    $csv = '"' + ($path -replace '"','""') + '","' + $ln + '","' + ($matches -replace '"','""') + '","' + ($ctx -replace '"','""') + '"'
    $csv | Out-File -FilePath $Out -Append -Encoding UTF8
  }

Write-Host 'scan-done, results:' $Out

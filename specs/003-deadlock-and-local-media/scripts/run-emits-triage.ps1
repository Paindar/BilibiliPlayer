# Triage emit-locks.csv into emit-locks-triage.csv with heuristic severity labels
$In = 'specs/003-deadlock-and-local-media/outputs/emit-locks.csv'
$Out = 'specs/003-deadlock-and-local-media/outputs/emit-locks-triage.csv'
if (-not (Test-Path $In)) { Write-Error "Input file not found: $In"; exit 1 }

$rows = Import-Csv -Path $In

# heuristics:
# - If 'match' contains 'emit' and context contains locking tokens -> NeedsReview
# - If 'match' contains 'emit' and context contains comment 'Emit' or 'Emit signals' -> LikelySafe
# - If 'match' does not contain 'emit' -> Info

$lockTokens = 'std::mutex','std::unique_lock','std::lock_guard','QMutexLocker','QMutex','QLockFile','std::lock_guard'

$result = @()
$counts = @{NeedsReview=0;LikelySafe=0;Info=0}
foreach ($r in $rows) {
  $match = $r.match
  $ctx = $r.context
  $sev = 'Info'
  if ($match -match 'emit') {
    $hasLock = $false
    foreach ($t in $lockTokens) { if ($ctx -match [regex]::Escape($t)) { $hasLock = $true; break } }
    if ($hasLock) {
      $sev = 'NeedsReview'
    } else {
      # check for comments that suggest intentional emit
      if ($ctx -match '(?i)\bEmit signals?\b' -or $ctx -match '(?i)//\s*Emit' -or $ctx -match '(?i)fallback: emit') {
        $sev = 'LikelySafe'
      } else {
        $sev = 'LikelySafe'
      }
    }
  } else {
    $sev = 'Info'
  }
  $counts[$sev] = $counts[$sev] + 1
  $obj = [PSCustomObject]@{
    file = $r.file
    line = $r.line
    match = $r.match
    context = $r.context
    severity = $sev
  }
  $result += $obj
}

# write out
$result | Export-Csv -Path $Out -NoTypeInformation -Encoding UTF8
Write-Host "Triage complete. Output: $Out"
Write-Host "Counts: NeedsReview=$($counts['NeedsReview']), LikelySafe=$($counts['LikelySafe']), Info=$($counts['Info'])"

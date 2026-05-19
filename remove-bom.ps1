# remove-bom.ps1
# Removes UTF-8 BOM (EF BB BF) from all *.rml files under assets/ recursively.

$files = Get-ChildItem -Path ".\assets" -Recurse | Where-Object { $_.Extension -in ".rml", ".rcss", ".txt" }

foreach ($file in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)

    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        $stripped = $bytes[3..($bytes.Length - 1)]
        [System.IO.File]::WriteAllBytes($file.FullName, $stripped)
        Write-Host "BOM removed: $($file.FullName)"
    } else {
        Write-Host "No BOM:      $($file.FullName)"
    }
}

Write-Host "`nDone."


param([string]$SourcePath, [string]$PdfPath)
$ErrorActionPreference = 'Stop'
$taskSource = (Resolve-Path -LiteralPath $SourcePath).Path
$taskPdf = [System.IO.Path]::GetFullPath($PdfPath)
$wordApplication = $null
$taskDocument = $null
try {
    $wordApplication = New-Object -ComObject Word.Application
    $wordApplication.Visible = $false
    $wordApplication.DisplayAlerts = 0
    $wordApplication.AutomationSecurity = 3
    $taskDocument = $wordApplication.Documents.Open($taskSource, $false, $false, $false)
    $taskDocument.Repaginate()
    $null = $taskDocument.Fields.Update()
    foreach ($taskToc in $taskDocument.TablesOfContents) { $taskToc.Update() }
    $taskDocument.Repaginate()
    foreach ($taskToc in $taskDocument.TablesOfContents) { $taskToc.UpdatePageNumbers() }
    $taskDocument.Save()
    $taskDocument.ExportAsFixedFormat($taskPdf, 17)
    Write-Output ('Paginas de Word: ' + $taskDocument.ComputeStatistics(2))
} finally {
    if ($null -ne $taskDocument) { $taskDocument.Close(0); [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($taskDocument) }
    if ($null -ne $wordApplication) { $wordApplication.Quit(); [void][System.Runtime.InteropServices.Marshal]::FinalReleaseComObject($wordApplication) }
}

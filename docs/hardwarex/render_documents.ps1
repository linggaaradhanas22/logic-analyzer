param([switch]$SkipWord, [switch]$SkipExcel, [switch]$WordOpenOnly, [switch]$TryWordPdf)
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out = Join-Path $root 'build\reports\project-documentation'
if (-not (Test-Path -LiteralPath $out)) { throw 'Document staging directory missing' }
$name = 'Logic Analyzer with Live Protocol Decoding_Lingga Aradhana Sahadewa'
$word = $null
$doc = $null
if (-not $SkipWord) { try {
    'Starting isolated Word automation'
    $word = New-Object -ComObject Word.Application
    'Word automation created'
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $word.AutomationSecurity = 3
    'Opening generated report read-only'
    $doc = $word.Documents.Open((Join-Path $out ($name + '.docx')), $false, $true)
    'Report opened; repaginating'
    $doc.Repaginate()
    'Word native open and pagination: {0} pages' -f $doc.ComputeStatistics(2)
    if ($TryWordPdf -and -not $WordOpenOnly) {
        'Exporting report PDF'
        $doc.SaveAs2((Join-Path $out ($name + '.pdf')), 17)
    }
} finally {
    if ($null -ne $doc) { $doc.Close(0); [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($doc) }
    if ($null -ne $word) { $word.Quit(); [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($word) }
} }
$excel = $null
$book = $null
if (-not $SkipExcel) { try {
    'Starting Excel automation'
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $book = $excel.Workbooks.Open((Join-Path $out 'KCCI Project_Documentation_Forms.xlsx'), 0, $true)
    for ($i = 1; $i -le $book.Worksheets.Count; $i++) {
        $sheet = $book.Worksheets.Item($i)
        if ($sheet.Name -eq 'Project Scoring Sheet') { continue }
        $sheet.PageSetup.Zoom = $false
        $sheet.PageSetup.FitToPagesWide = 1
        $sheet.PageSetup.FitToPagesTall = 1
        $sheet.ExportAsFixedFormat(0, (Join-Path $out ('kcci-' + $i + '.pdf')))
        'Excel native render: {0}' -f $sheet.Name
    }
} finally {
    if ($null -ne $book) { $book.Close($false); [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($book) }
    if ($null -ne $excel) { $excel.Quit(); [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
} }

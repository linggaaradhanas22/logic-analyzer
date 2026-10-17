param(
    [string]$DeckName = 'logic-analyzer-design.pptx',
    [string]$PdfName = 'logic-analyzer-design-powerpoint.pdf',
    [string]$PreviewPrefix = 'powerpoint'
)
$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out = Join-Path $root 'build\presentation'
if (-not (Test-Path -LiteralPath $out)) { throw 'Presentation output directory missing' }
$preview = Join-Path $out 'previews'
if (-not (Test-Path -LiteralPath $preview)) { throw 'Preview directory missing' }
$app = $null
$deck = $null
try {
    $app = New-Object -ComObject PowerPoint.Application
    $deck = $app.Presentations.Open((Join-Path $out $DeckName), -1, 0, 0)
    if ($deck.SlideShowSettings.NamedSlideShows.Count -ne 2) { throw 'Expected two subsystem custom slide shows' }
    $expectedShows = @{ 'FPGA system' = 22; 'Embedded system' = 25 }
    for ($j = 1; $j -le $deck.SlideShowSettings.NamedSlideShows.Count; $j++) {
        $show = $deck.SlideShowSettings.NamedSlideShows.Item($j)
        # The COM SAFEARRAY exposes a leading zero slot through PowerShell.
        # Real PowerPoint slide IDs are positive; validate all real entries.
        $ids = @($show.SlideIDs | Where-Object { $_ -gt 0 })
        'Native custom show: {0} ({1} slides)' -f $show.Name, $ids.Count
        if (-not $expectedShows.ContainsKey($show.Name) -or $ids.Count -ne $expectedShows[$show.Name]) { throw ('Unexpected native custom show route: {0}, count {1}' -f $show.Name, $ids.Count) }
        if ($show.Name -eq 'FPGA system') { $positions = @(1..18) + @(34..37) } else { $positions = @(1..6) + @(19..37) }
        $expectedIds = @(foreach ($position in $positions) { $deck.Slides.Item($position).SlideID })
        if (($ids -join ',') -ne ($expectedIds -join ',')) { throw 'Native custom show has the wrong slide sequence' }
    }
    $deck.SaveAs((Join-Path $out $PdfName), 32)
    for ($i = 1; $i -le $deck.Slides.Count; $i++) {
        $dest = Join-Path $preview ('{0}-{1:D2}.png' -f $PreviewPrefix, $i)
        $deck.Slides.Item($i).Export($dest, 'PNG', 1920, 1080)
    }
    'PowerPoint native render PASS: {0} slides, PDF and 1920x1080 PNG previews' -f $deck.Slides.Count
} finally {
    if ($null -ne $deck) { $deck.Close(); [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($deck) }
    if ($null -ne $app) { $app.Quit(); [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($app) }
}

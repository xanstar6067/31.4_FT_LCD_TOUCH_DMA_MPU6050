param(
    [Parameter(Mandatory = $true)]
    [string]$BdfPath,
    [string]$Output = "Core/Inc/font_10x18_menu.inc",
    [string]$Preview = "tools/font_10x18_menu_preview.png"
)

# Source used for the checked-in subset:
# https://terminus-font.sourceforge.net/ (version 4.49.1, ter-u18b.bdf)

Add-Type -AssemblyName System.Drawing

$glyphWidth = 10
$glyphHeight = 18
$glyphs = @{}
$encoding = -1
$bitmapRows = $null
$readingBitmap = $false

foreach ($line in [System.IO.File]::ReadLines((Resolve-Path $BdfPath))) {
    if ($line.StartsWith("ENCODING ")) {
        $encoding = [int]$line.Substring(9)
    } elseif ($line -eq "BITMAP") {
        $bitmapRows = New-Object System.Collections.Generic.List[uint16]
        $readingBitmap = $true
    } elseif ($line -eq "ENDCHAR") {
        if (($encoding -ge 0) -and ($bitmapRows -ne $null)) {
            if ($bitmapRows.Count -ne $glyphHeight) {
                throw "U+$('{0:X4}' -f $encoding) has $($bitmapRows.Count) rows; expected $glyphHeight."
            }
            $glyphs[$encoding] = [uint16[]]$bitmapRows.ToArray()
        }
        $encoding = -1
        $bitmapRows = $null
        $readingBitmap = $false
    } elseif ($readingBitmap) {
        $bitmapRows.Add([Convert]::ToUInt16($line, 16))
    }
}

$asciiCodepoints = @(32..126)
$cyrillicCodepoints =
    @(0x0410..0x0415) + @(0x0401) + @(0x0416..0x042F)

foreach ($codepoint in ($asciiCodepoints + $cyrillicCodepoints)) {
    if (-not $glyphs.ContainsKey($codepoint)) {
        throw "The BDF file does not contain U+$('{0:X4}' -f $codepoint)."
    }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("/* 10x18 menu subset derived from Terminus Font 4.49.1 Bold. */")
$lines.Add("/* Copyright (C) 2020 Dimitar Toshkov Zhekov. SIL OFL 1.1. */")
$lines.Add("/* See LICENSES/Terminus-OFL-1.1.txt. */")
$lines.Add("static const uint16_t Font10x18MenuAscii[] = {")

foreach ($codepoint in $asciiCodepoints) {
    $values = $glyphs[$codepoint] | ForEach-Object {
        '0x{0:X4}' -f $_
    }
    $lines.Add("    " + ($values -join ', ') +
               (', /* U+{0:X4} */' -f $codepoint))
}
$lines.Add("};")
$lines.Add("")
$lines.Add("static const uint16_t Font10x18MenuCyrillic[] = {")

foreach ($codepoint in $cyrillicCodepoints) {
    $values = $glyphs[$codepoint] | ForEach-Object {
        '0x{0:X4}' -f $_
    }
    $lines.Add("    " + ($values -join ', ') +
               (', /* U+{0:X4} */' -f $codepoint))
}
$lines.Add("};")

$scale = 6
$columns = 11
$rows = [Math]::Ceiling($cyrillicCodepoints.Count / $columns)
$previewBitmap = New-Object System.Drawing.Bitmap(
    ($columns * $glyphWidth * $scale),
    ($rows * $glyphHeight * $scale))
$graphics = [System.Drawing.Graphics]::FromImage($previewBitmap)
$graphics.Clear([System.Drawing.Color]::FromArgb(16, 22, 28))

for ($index = 0; $index -lt $cyrillicCodepoints.Count; $index++) {
    $cellX = ($index % $columns) * $glyphWidth * $scale
    $cellY = [Math]::Floor($index / $columns) * $glyphHeight * $scale
    $rowsForGlyph = $glyphs[$cyrillicCodepoints[$index]]

    for ($y = 0; $y -lt $glyphHeight; $y++) {
        for ($x = 0; $x -lt $glyphWidth; $x++) {
            if (($rowsForGlyph[$y] -band (0x8000 -shr $x)) -ne 0) {
                $graphics.FillRectangle(
                    [System.Drawing.Brushes]::White,
                    $cellX + ($x * $scale),
                    $cellY + ($y * $scale),
                    $scale,
                    $scale)
            }
        }
    }
}

$outputPath = Join-Path (Get-Location) $Output
$previewPath = Join-Path (Get-Location) $Preview
[System.IO.File]::WriteAllLines(
    $outputPath,
    $lines,
    (New-Object System.Text.UTF8Encoding($false)))
$previewBitmap.Save($previewPath, [System.Drawing.Imaging.ImageFormat]::Png)

$graphics.Dispose()
$previewBitmap.Dispose()

Write-Output "Generated $outputPath"
Write-Output "Preview   $previewPath"

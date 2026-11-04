[CmdletBinding()]
param(
    [ValidateSet('RelWithDebInfo', 'Release')][string]$Configuration = 'RelWithDebInfo',
    [string]$OutputDirectory = 'dist',
    [string]$MSBuildPath = 'msbuild',
    [string]$CMakePath = 'cmake'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $spec = Get-Content buildspec.json -Raw | ConvertFrom-Json
    $version = $spec.version
    if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'MSI requiere una versión major.minor.build.' }
    $out = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
        [IO.Path]::GetFullPath($OutputDirectory)
    } else { [IO.Path]::GetFullPath((Join-Path $projectRoot $OutputDirectory)) }
    $stage = Join-Path $out "package-$version"
    & $CMakePath --install build_x64 --config $Configuration --prefix $stage
    if ($LASTEXITCODE) { throw 'Falló cmake --install.' }
    Copy-Item scripts/Install-Plugin.ps1,README.md,README.es.md,VALIDATION.md,LICENSE $stage -Force
    Copy-Item docs -Destination $stage -Recurse -Force
    # One product identity per release across both installer languages and rebuilds.
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $identity = $hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes("org.obsmediamtx.plugin/msi/x64/$version")) }
    finally { $hasher.Dispose() }
    $productCode = [Guid]::new([byte[]]$identity[0..15]).ToString()
    # Los símbolos de depuración se distribuyen aparte, no en el instalador.
    foreach ($culture in 'en-US','es-ES') {
      & $MSBuildPath installer/VillaCast.wixproj /restore /t:Rebuild /nologo /v:minimal `
        /p:Configuration=Release "/p:PluginVersion=$version" `
        "/p:ProductCode=$productCode" `
        "/p:Cultures=$culture" `
        "/p:PayloadDir=$stage\obs-mediamtx" "/p:OutputPath=$out\"
      if ($LASTEXITCODE) { throw "Falló la compilación del MSI ($culture)." }
      $filename = "VillaCast-$version-windows-x64-$culture.msi"
      Copy-Item -LiteralPath (Join-Path $out "$culture/$filename") -Destination (Join-Path $out $filename) -Force
    }
    $name = "VillaCast-$version-windows-x64"
    Compress-Archive -Path "$stage/*" -DestinationPath "$out/$name-portable.zip" -Force
    Get-FileHash "$out/$name-en-US.msi","$out/$name-es-ES.msi","$out/$name-portable.zip" -Algorithm SHA256 |
        ForEach-Object { '{0}  {1}' -f $_.Hash.ToLowerInvariant(), [IO.Path]::GetFileName($_.Path) } |
        Set-Content "$out/$name-SHA256SUMS.txt" -Encoding ascii
    Write-Output "MSI, ZIP y SHA256SUMS: $out"
} finally { Pop-Location }

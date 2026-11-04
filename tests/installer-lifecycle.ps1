#Requires -RunAsAdministrator
[CmdletBinding()]
param([Parameter(Mandatory)][string]$MSBuildPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (Get-Process obs64 -ErrorAction SilentlyContinue) { throw 'Cierra OBS para las pruebas.' }
$testRoot = Join-Path $projectRoot 'dist/msi-lifecycle'
$installRoot = Join-Path $testRoot 'installed'
$version = (Get-Content (Join-Path $projectRoot 'buildspec.json') -Raw | ConvertFrom-Json).version
$payload = Join-Path $projectRoot "dist/package-$version/obs-mediamtx"
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
Start-Transcript -Path (Join-Path $testRoot 'transcript.txt') -Force | Out-Null
function Invoke-Msi([string[]]$Arguments, [string]$Name, [int]$Expected = 0) {
    $log = Join-Path $testRoot "$Name.log"
    $process = Start-Process msiexec.exe -ArgumentList ($Arguments + @('/qn','/norestart','/L*v',('"'+$log+'"'))) -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode -ne $Expected) { throw "$Name devolvió $($process.ExitCode), se esperaba $Expected. Registro: $log" }
    Write-Output "$Name : correcto ($Expected)"
}
Push-Location $projectRoot
try {
    foreach ($testVersion in '1.0.0','1.0.1') {
        $out = Join-Path $testRoot $testVersion
        $culture = if ($testVersion -eq '1.0.0') { 'en-US' } else { 'es-ES' }
        & $MSBuildPath installer/VillaCast.wixproj /restore /t:Rebuild /nologo /v:minimal `
            /p:Configuration=Release "/p:PluginVersion=$testVersion" `
            "/p:Cultures=$culture" `
            /p:ProductName=VillaCast-Installer-Test `
            /p:UpgradeCode=688CB4C7-E95B-4C5A-9B82-7E1328101F28 `
            "/p:PayloadDir=$payload" "/p:OutputPath=$out\"
        if ($LASTEXITCODE) { throw 'Falló la compilación de prueba.' }
    }
    $first = Join-Path $testRoot '1.0.0/en-US/VillaCast-1.0.0-windows-x64-en-US.msi'
    $second = Join-Path $testRoot '1.0.1/es-ES/VillaCast-1.0.1-windows-x64-es-ES.msi'
    $target = 'INSTALLFOLDER="'+$installRoot+'"'
    Invoke-Msi @('/i',('"'+$first+'"'),$target) 'install'
    $installedDll = Join-Path $installRoot 'bin/64bit/obs-mediamtx.dll'
    if ((Get-FileHash $installedDll).Hash -ne (Get-FileHash "$payload/bin/64bit/obs-mediamtx.dll").Hash) { throw 'DLL incorrecta.' }
    Set-Content (Join-Path $installRoot 'preserve-settings.txt') 'keep'
    Invoke-Msi @('/i',('"'+$second+'"'),$target) 'upgrade'
    Invoke-Msi @('/i',('"'+$first+'"'),$target) 'downgrade-blocked' 1603
    Remove-Item -LiteralPath $installedDll
    Invoke-Msi @('/fa',('"'+$second+'"'),$target) 'repair'
    if ((Get-FileHash $installedDll).Hash -ne (Get-FileHash "$payload/bin/64bit/obs-mediamtx.dll").Hash) { throw 'Reparación incorrecta.' }
    Invoke-Msi @('/x',('"'+$second+'"')) 'uninstall'
    if (Test-Path -LiteralPath $installedDll) { throw 'La DLL sigue instalada.' }
    if ((Get-Content (Join-Path $installRoot 'preserve-settings.txt')) -ne 'keep') { throw 'Se borraron archivos ajenos.' }
    Write-Output 'Ciclo MSI completo: instalación, actualización, bloqueo de downgrade, reparación y desinstalación.'
} finally { Pop-Location; Stop-Transcript | Out-Null }

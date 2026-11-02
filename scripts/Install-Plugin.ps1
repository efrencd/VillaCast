#Requires -RunAsAdministrator
[CmdletBinding()]
param(
    [string]$PluginSource = (Join-Path $PSScriptRoot 'obs-mediamtx'),
    [string]$DestinationRoot = (Join-Path $env:ProgramData 'obs-studio\plugins')
)
$ErrorActionPreference = 'Stop'
if (Get-Process obs64 -ErrorAction SilentlyContinue) {
    throw 'Cierra todas las instancias de OBS antes de instalar o actualizar el plugin.'
}
$pluginDll = Join-Path $PluginSource 'bin\64bit\obs-mediamtx.dll'
if (!(Test-Path -LiteralPath $pluginDll)) {
    throw 'No se encuentra la DLL junto al instalador. Extrae todo el ZIP antes de ejecutarlo.'
}
$pluginDestination = Join-Path $DestinationRoot 'obs-mediamtx'
New-Item -ItemType Directory -Path $pluginDestination -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $PluginSource 'bin') -Destination $pluginDestination -Recurse -Force
Copy-Item -LiteralPath (Join-Path $PluginSource 'data') -Destination $pluginDestination -Recurse -Force
Write-Host "Instalado en $pluginDestination"
Write-Host 'Abre OBS y activa Paneles > MediaMTX · WebRTC. El servidor se inicia manualmente desde el panel.'

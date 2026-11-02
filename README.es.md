# OBS MediaMTX

Plugin nativo de OBS con panel de control de MediaMTX y salida WebRTC independiente.
Versión 0.6.1 de desarrollo; consultar `VALIDATION.md` antes de instalar.

## Instalador de Windows

Descargar `obs-mediamtx-VERSION-windows-x64-es-ES.msi` desde **GitHub Releases**, cerrar OBS
y abrir el MSI. Hay asistentes en español (-es-ES.msi) e inglés (-en-US.msi), con permisos de administrador.
Instala para todos los usuarios en `%PROGRAMDATA%\obs-studio\plugins\obs-mediamtx`.
Requiere una instalación normal de OBS Studio de 64 bits (OBS 31.1 o posterior).
El ZIP `obs-mediamtx-VERSION-windows-x64-portable.zip` sigue disponible para instalación
manual y OBS portable. El MSI detecta la instalación de OBS mediante su registro de Windows.

Para actualizar, ejecutar el MSI de la nueva versión. No admite volver a una versión anterior
sin desinstalar primero. Se puede reparar o desinstalar mediante Windows Installer;
la desinstalación aparece en Aplicaciones de Windows. Los ajustes personales de OBS y del
plugin se conservan. MediaMTX se instala o actualiza por separado desde el panel del plugin;
el MSI no lo instala, desinstala ni cambia el cortafuegos o el inicio de Windows.

Instalación administrada (terminal elevada):

```powershell
msiexec /i "obs-mediamtx-0.6.1-windows-x64-es-ES.msi" /qn /norestart /L*v "instalacion.log"
msiexec /x "obs-mediamtx-0.6.1-windows-x64-es-ES.msi" /qn /norestart
```

## Generar MSI y preparar GitHub Releases

Con el plugin compilado, ejecutar en una terminal de desarrollo de Visual Studio:

```powershell
./scripts/Package-Windows.ps1
```

WiX Toolset SDK y extensiones UI/Util 6.0.2 se restauran desde NuGet mediante MSBuild;
no se necesita instalar WiX o .NET SDK en el ordenador del usuario final. El script genera
en `dist` los MSI en español e inglés, el ZIP portable y `SHA256SUMS`. WiX tiene sus propios términos de uso y
mantenimiento, publicados en https://www.nuget.org/packages/WixToolset.Sdk/6.0.2.
No cambiar el UpgradeCode de `installer/OBS-MediaMTX.wixproj` entre versiones.
Incrementar los tres campos de versión de `buildspec.json` para cada actualización publicada.

Las Releases se generan y publican manualmente. No se incluyen workflows de GitHub Actions. Linux y macOS quedan pendientes de implementar y validar.
El MSI generado aún no lleva firma
Authenticode; añadir firma con certificado antes de una distribución pública definitiva.

Prueba local de MSI (terminal elevada, después de generar los paquetes):

```powershell
./tests/installer-lifecycle.ps1 -MSBuildPath "RUTA_A_MSBuild.exe"
```

Usa otra identidad de producto y una carpeta bajo `dist`, sin reemplazar el plugin habitual.

## Uso

1. Abrir **Paneles → MediaMTX · WebRTC**.
2. Seleccionar el ejecutable existente, o pulsar **Instalar MediaMTX** (Windows/WinGet).
3. Ajustar nombre, puertos, acceso LAN, resolución, bitrate y codificador.
4. Seleccionar **WebRTC** en **Tipo de emisión** y pulsar **Iniciar**: espera al servidor y emite automáticamente.
5. Abrir o copiar el enlace del panel. Con mDNS activo se muestra un nombre `.local`.
   OBS envía su programa y la mezcla de audio 1.
6. **Detener** detiene primero la salida y después el proceso de MediaMTX que inició el plugin.

La salida de transmisión y la grabación principales de OBS se mantienen independientes.
La salida adicional consume recursos de codificación. Los FPS son los de OBS; por defecto
se usa la resolución de salida de OBS, x264 y 4000 kbps, H.264 sin B-frames y audio Opus a 128 kbps.
También puede seleccionarse NVENC H.264 en equipos compatibles. Mantener una fuente de audio activa en la mezcla 1, aunque esté silenciada.

No inicia OBS con Windows. La casilla **Iniciar automáticamente al abrir OBS**, desactivada por defecto, arranca servidor y emisión con los ajustes guardados. No cierra servidores externos.
Si hay otro MediaMTX usando los puertos, muestra un error: detener manualmente esa instancia
o escoger puertos distintos. No cambia el perfil de emisión ni los archivos anteriores.

## Instalación y actualización del servidor

En Windows se utiliza el paquete exacto `bluenviron.mediamtx` del origen `winget`.
Se muestra una confirmación con los acuerdos antes de iniciar la operación. WinGet se
ejecuta sin bloquear la interfaz y su salida se muestra en el panel. No se actualiza durante
la emisión ni con el servidor administrado en marcha. Si falta WinGet, instalar App Installer
o seleccionar una descarga oficial de MediaMTX manualmente.

Se requiere **MediaMTX 1.21 o posterior** para la configuración generada. Las instalaciones
manuales también se admiten. Una actualización de WinGet es global a esa instalación del
servidor; no modifica únicamente la copia del plugin.

## Configuración y red

El plugin guarda `settings.ini` y `mediamtx.yml` en la carpeta de configuración de módulo
que proporciona OBS. **Abrir configuración** muestra esa carpeta. Al iniciar el servidor se
genera de nuevo el YAML desde los campos del panel; las ediciones manuales se sobrescriben.

Solo se permite publicar desde loopback. La lectura puede limitarse al equipo o permitirse
a direcciones privadas IPv4. No se habilita API remota ni otros protocolos. En modo LAN,
Windows debe permitir el ejecutable de MediaMTX en la red privada: HTTP TCP (8889 por defecto)
y medios UDP/TCP (8189). El proceso OBS sirve el reproductor en TCP 8891 por defecto. El plugin no cambia el cortafuegos ni el router.

## Nombre estable sin controlar el router

En **Ajustes**, activar **Permitir lectura desde la red local** y **Anunciar un nombre .local**.
Escribir solo la primera parte del nombre, por ejemplo `obs-aula`, sin `.local`, y elegir
Ethernet o Wi-Fi en **Interfaz para el enlace**. Cambiar estos ajustes con el servidor detenido.
En modo automático se prioriza Ethernet, después Wi-Fi. La interfaz explícita evita elegir
accidentalmente una VPN o un adaptador virtual.

Al iniciar MediaMTX, el plugin comprueba que el nombre está libre y anuncia, por ejemplo,
`http://obs-aula.local:8891/`. El nombre y la interfaz se guardan. No se instala
Bonjour ni se modifica DNS, hosts, DHCP, router o inicio de Windows. El motor mDNS usa
Qt Network y publica únicamente la dirección IPv4 de la interfaz elegida.

El panel solo muestra el nombre como enlace principal cuando el anuncio está activo; se
conserva un segundo enlace por IP. Si cambia la dirección de esa interfaz, el plugin retira
el anuncio anterior y comprueba/anuncia la nueva dirección (revisión cada cinco segundos).
El reproductor del plugin reintenta la conexión tras un corte o cambio de red. Los enlaces directos antiguos de MediaMTX pueden necesitar recargar la página.
Un nombre duplicado desactiva el anuncio y muestra un aviso: elegir otro nombre y reiniciar
el servidor. Al detener MediaMTX o cerrar OBS se retira el anuncio.

Se requiere comunicación multicast mDNS (UDP 5353) entre los clientes y OBS en el mismo
segmento local, además de los puertos de MediaMTX. En redes escolares con aislamiento de
clientes, VLAN distintas o multicast bloqueado, hace falta que la administración de red
permita o retransmita mDNS; el plugin no puede garantizarlo ni sustituir esa configuración.
No cambia el cortafuegos automáticamente. La compatibilidad debe probarse desde el navegador
del dispositivo cliente. Si la red cambia, no volverá a requerirse conocer la nueva IP.

También se permite lectura desde IPv6 local (loopback, link-local y ULA) para que el uso del
nombre normal del equipo no cause una petición de credenciales solo por escoger IPv6.
La publicación sigue limitada a este PC. El nombre `.local` del plugin anuncia IPv4.

Internet, HTTPS, TURN, autenticación de espectadores y QR quedan para versiones posteriores.
HLS tampoco forma parte de esta primera salida: necesita validar por separado la compatibilidad
de audio de los reproductores y la posible codificación adicional.

## Compilar Windows x64

Requisitos: Visual Studio 2022 Build Tools con C++, Windows SDK, Git y CMake 3.28 o posterior.
La base de compilación procede de la [plantilla oficial de OBS](https://github.com/obsproject/obs-plugintemplate).
Las dependencias oficiales de OBS/Qt están fijadas por versión y SHA-256 en `buildspec.json`.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
cmake --install build_x64 --config RelWithDebInfo --prefix ./dist
```

El directorio `dist/obs-mediamtx` contiene `bin/64bit/obs-mediamtx.dll` y los datos del módulo.
Con OBS cerrado, copiar esa carpeta a `%PROGRAMDATA%/obs-studio/plugins/obs-mediamtx`.
No copiar las bibliotecas de desarrollo de OBS ni las DLL de Qt sobre las de OBS.

## Portabilidad

- `src/platform.cpp`: detección de ejecutable y gestor de paquetes; WinGet solo bajo `Q_OS_WIN`.
- `src/plugin-main.cpp`: panel Qt, proceso administrado y salida propia de libobs.
- `src/mdns.cpp`: anunciador de nombres IPv4 mDNS con Qt; código común para futuros puertos.
- El servidor se inicia con argumentos estructurados, sin PowerShell ni comandos de shell.
- Linux/macOS necesitarán sus propios paquetes, validación ABI, pruebas y adaptadores de
  instalación. El código común no implica que un binario Windows funcione en otros sistemas.

Licencia GPL-2.0-or-later, como la plantilla de OBS. MediaMTX se instala por separado y
conserva su propia licencia.

## Pruebas mDNS

`ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure` comprueba validación de
nombres, formato de consultas/respuestas, compresión DNS, paquetes truncados y límites.
La integración de red es optativa y no se ejecuta automáticamente:

```text
python tests/mdns-network.py RUTA_A_mdns-tests.exe IP_LOCAL INDICE_INTERFAZ
```

Requiere las DLL de Qt en PATH. Usa nombres aleatorios de prueba, comprueba respuestas A,
negación AAAA, retirada al detener y detección de un conflicto de nombre. No modifica
interfaces ni reglas del cortafuegos. Windows puede mostrar un aviso para el ejecutable de pruebas.

El ZIP incluye Install-Plugin.ps1. Extraer todo el paquete y ejecutarlo en PowerShell como administrador, con OBS cerrado. Solo copia el plugin; no instala MediaMTX ni modifica el inicio de Windows.


## Reproductor con reconexión (0.3.0)

El enlace principal del panel usa ahora **8891** por defecto, por ejemplo
`http://tvescolar.local:8891/`. Este reproductor adapta la imagen completa a la ventana
sin recortarla y vuelve a conectar si se detiene/reinicia la emisión o MediaMTX.
El cliente debe abrir una vez este nuevo enlace. Los enlaces antiguos al puerto 8889
siguen siendo el reproductor directo de MediaMTX y no reciben la reconexión del plugin.

Se conserva WebRTC/WHEP en 8889 y los medios ICE en 8189. Puede cambiarse el puerto del
reproductor en Ajustes, con el servidor detenido. Los tres puertos deben ser distintos.
La página se sirve solo mientras el servidor administrado está encendido; una página
ya abierta mantiene sus reintentos aunque el servidor se detenga. Se requiere instalar
la carpeta data completa (incluido viewer.html), además de la DLL.

El escalado de vídeo se hace por GPU mediante el mecanismo de mezclas de OBS. Por ejemplo,
con lienzo de 1920×1080 y salida principal de 1280×720, el plugin puede generar su propia
salida de 1920×1080 sin cambiar la principal. NVENC utiliza el ajuste ull de baja latencia;
x264 conserva zerolatency. El registro del panel también aparece en el registro de OBS.

## Control unificado y resolución automática (0.4.0)

Iniciar arranca servidor y emisión; Detener apaga ambos. El desplegable de tipo de emisión
contiene WebRTC. El inicio espera a que responda MediaMTX y cancela ante errores o parada.
En Ajustes, Resolución usa por defecto la salida de vídeo de OBS (no el lienzo), también
al actualizar desde versiones anteriores. Se lee en cada arranque de emisión. Personalizada
permite escoger ancho y alto con el escalado GPU existente. Los FPS siguen siendo los de OBS.
El registro distingue fecha azul y hora verde e incluye marcas de tiempo en las filas del plugin.




## Posición del panel (0.5.1)

El panel se registra antes de que OBS restaure su distribución. OBS conserva su posición,
tamaño y visibilidad entre sesiones. El inicio automático de servidor y emisión espera
a que OBS termine de cargar. Tras actualizar, colocar el panel y cerrar OBS normalmente
una vez para guardar la distribución elegida.


## Idiomas (0.6.0)

El plugin utiliza español si OBS está en español o si el idioma principal de interfaz
del sistema es español; en otro caso utiliza inglés. Se incluyen variantes regionales
como es-MX y es_AR. La selección se realiza al abrir OBS, sin cambiar el idioma del propio OBS.
Los catálogos están en data/locale/es-ES.ini y en-US.ini; las claves ausentes en español
recurren al texto inglés. El panel, sus mensajes y el reproductor web siguen esta elección.
Los registros originales de MediaMTX y mensajes del sistema conservan su idioma de origen.
Los MSI tienen idiomas fijos, indicados en el nombre de cada descarga, y ambos incluyen
los dos catálogos. Solo hay que instalar uno de ellos. El ZIP también incluye ambos.

English documentation: [README.md](README.md).



## Autoría y origen

Autor: **Efren C.D.** Desarrollado originalmente para el proyecto de TV escolar de
**Villaverde, Fuerteventura, Islas Canarias**. La pestaña Acerca de muestra la versión
de la compilación y el enlace del proyecto: https://github.com/efrencd/OBS-MediaMTX.

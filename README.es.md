# VillaCast

<img src="data/villacast.png" width="128" alt="Logo VillaCast: vídeo distribuido a pantallas en red">

**Convierte OBS en un canal de televisión en directo para tu red local.**

[English](README.md) · [Descargar instaladores](https://github.com/efrencd/VillaCast/releases/latest) · [Manual de uso](docs/MANUAL.es.md)

VillaCast es un plugin nativo de OBS Studio que envía la imagen del programa a
navegadores mediante WebRTC. Desde un único panel puedes iniciar el servidor y una
emisión independiente, compartir el enlace con las aulas y supervisar las conexiones.
MediaMTX se ocupa de distribuir el vídeo; VillaCast integra su configuración y sus
controles de uso diario en OBS.

Desarrollado originalmente por **Efren C.D.** para el **proyecto de TV escolar de
Villaverde, en Fuerteventura, Islas Canarias**.

## Funciones

- Emisión WebRTC H.264/Opus independiente de la transmisión y grabación principales de OBS.
- Botón único Iniciar/Detener con indicador verde de emisión que pulsa suavemente; inicio automático opcional al abrir OBS.
- Instalación y actualización de MediaMTX mediante WinGet o selección de una instalación existente.
- Resolución y FPS de la salida de OBS, dimensiones personalizadas y codificadores x264/NVIDIA NVENC.
- Enlace por IP o nombre `.local` opcional en redes compatibles.
- Reproductor configurable: título, fondo, ajuste de imagen, sonido, reproducción automática, modo directo y reconexión.
- Botones con iconos y barra que se oculta; doble clic para alternar pantalla completa incluso sin barra.
- Clientes con número de fila, nombre de red disponible y estadísticas de recepción.
- Pestaña LOG separada, panel adaptable, guardado automático e interfaz en español e inglés.

## Descargar e instalar — Windows x64

Versión actual: **0.8.1**, una versión inicial. Descarga desde
[GitHub Releases](https://github.com/efrencd/VillaCast/releases/latest):

| Archivo | Uso |
| --- | --- |
| `VillaCast-0.8.1-windows-x64-es-ES.msi` | Instalador con asistente en español |
| `VillaCast-0.8.1-windows-x64-en-US.msi` | Instalador con asistente en inglés |
| `VillaCast-0.8.1-windows-x64-portable.zip` | Archivos del plugin y script de instalación |
| `VillaCast-0.8.1-windows-x64-SHA256SUMS.txt` | Sumas SHA-256 para comprobar las descargas |

Ambos MSI incluyen el mismo plugin bilingüe: **instala solo uno**. Cierra OBS, ejecuta
el instalador con permisos de administrador y vuelve a abrir OBS. Activa
**Paneles → VillaCast**. Para OBS portable, consulta el [manual](docs/MANUAL.es.md#instalación).
Los instaladores todavía no están firmados. MediaMTX se instala por separado desde el panel.

**Requisitos:** Windows x64, OBS Studio 31.1 o posterior con WHIP/Opus y MediaMTX 1.21
o posterior. Las pruebas locales utilizan OBS 33.0.1 y MediaMTX 1.21.3. NVENC requiere
una GPU NVIDIA y controlador compatibles. Aún no hay versiones para Linux/macOS.

## Primera emisión

1. Abre **Paneles → VillaCast** e instala o selecciona MediaMTX.
2. En **Ajustes**, selecciona codificador e interfaz de red. La resolución automática sigue la salida de vídeo de OBS.
3. Mantén una fuente de audio activa en la mezcla 1 de OBS, aunque esté silenciada.
4. Pulsa **Iniciar**: se ponen en marcha el servidor y la emisión independiente.
5. Prueba **Abrir vídeo** o utiliza **Copiar enlace** para compartirlo con las aulas.
6. Consulta **Clientes** y **LOG**. Pulsa **Detener** para terminar la emisión y el servidor.

El reproductor usa el puerto 8000 por defecto en instalaciones nuevas; se conservan
los puertos ya guardados.

El navegador puede exigir un clic para activar el sonido o iniciar la reproducción.
Los ajustes se guardan automáticamente; recarga las páginas abiertas para aplicar
cambios del reproductor. Al cerrar OBS se detiene el servidor iniciado por VillaCast.
El inicio automático solo actúa al abrir OBS; no añade nada al inicio de Windows.

## Red y supervisión

Puertos predeterminados: reproductor TCP **8000**, señalización WebRTC TCP **8889**,
vídeo/audio UDP/TCP **8189** y API de supervisión **127.0.0.1:9997**, protegida con una
credencial aleatoria por arranque. El nombre `.local` depende de mDNS. El aislamiento
Wi-Fi, las VLAN o el cortafuegos pueden impedir el acceso. VillaCast no cambia el
router, el cortafuegos ni el DNS del colegio. Esta versión está orientada a la red
local; no incluye un asistente de publicación en Internet/TURN.

Las estadísticas detalladas combinan pequeños informes periódicos del reproductor
web de VillaCast con la API local de MediaMTX. Los reproductores externos muestran
los datos básicos de sesión. Los informes permanecen en memoria y caducan. Los nombres
de red se resuelven en segundo plano con caché y pueden no estar disponibles. RTT no
mide el retardo total del vídeo. El [manual](docs/MANUAL.es.md) explica cada control y columna.

## Actualizar desde OBS MediaMTX

Instala VillaCast sobre la versión anterior con OBS cerrado. Se conservan los ajustes,
la identidad del panel, la identidad de actualización MSI y las rutas internas
`obs-mediamtx`. Los títulos personalizados se mantienen; el antiguo título por defecto
pasa a VillaCast. No instales una segunda copia en otra carpeta. La desinstalación
conserva los ajustes personales y MediaMTX, que se instala por separado.

## Compilación

Necesitas Visual Studio 2022 C++ Build Tools, Windows SDK, Git y CMake 3.28+:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
./scripts/Package-Windows.ps1
```

El empaquetado utiliza WiX SDK/UI/Util 6.0.2 mediante MSBuild y genera MSI en ambos
idiomas, ZIP con los manuales y sumas de comprobación. Las publicaciones son manuales;
no se incluyen flujos GitHub Actions. Consulta [las pruebas y límites](VALIDATION.md).
Licencia: [GPL-2.0-or-later](LICENSE).

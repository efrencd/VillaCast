# VillaCast 0.8.3 — Manual de uso

Para escuchar solo el audio del programa, añade `?audio=1` al enlace, por ejemplo `http://estudio:8000/?audio=1`. Pulsa el control de sonido si el navegador requiere un toque.

[English](MANUAL.en.md) · [Proyecto](../README.es.md) · [Descargas](https://github.com/efrencd/VillaCast/releases/latest)

VillaCast envía a los navegadores el **programa de OBS** (no la previsualización del
modo Estudio) y la mezcla de audio 1 mediante una salida WebRTC independiente.
Los botones principales de transmisión y grabación de OBS controlan otras salidas.
El selector de emisión solo incluye WebRTC; RTSP/HLS no son modos integrados en esta versión.

## Instalación

1. Instala OBS Studio 31.1 o posterior para Windows x64, con WHIP y Opus.
2. Cierra todas las instancias de OBS y descarga un MSI de VillaCast desde Releases.
   Elige el idioma del asistente; ambos incluyen el mismo plugin bilingüe.
3. Ejecuta el MSI con permisos de administrador, abre OBS y activa **Paneles → VillaCast**.
4. Instala MediaMTX mediante WinGet desde VillaCast o selecciona un ejecutable MediaMTX 1.21+ existente.
5. Configura tu escena y mantén una fuente de audio activa en la mezcla 1, aunque esté silenciada.
6. Inicia VillaCast, prueba el reproductor en este PC y después el enlace en otro dispositivo.

Para OBS portable, ciérralo, extrae el ZIP y copia
`obs-mediamtx/bin/64bit/obs-mediamtx.dll` a `obs-plugins/64bit/obs-mediamtx.dll`, y
el contenido de `obs-mediamtx/data` a `data/obs-plugins/obs-mediamtx/` dentro de OBS
portable. El script `Install-Plugin.ps1` del ZIP, ejecutado como administrador, instala
en la carpeta de plugins del sistema. No mantengas copias duplicadas. El nombre
interno `obs-mediamtx` se conserva para actualizar las versiones anteriores.

## Guardado, idioma y distribución del panel

Los cambios se guardan automáticamente: las casillas al cambiarlas; los campos de
texto, números y selecciones tras unos 0,5 segundos de pausa. Si el fondo es inválido,
se indica el error; corrígelo al formato `#RRGGBB`. **Recarga las páginas abiertas**
para aplicar cambios del reproductor. Detén VillaCast para cambiar red/codificación;
los controles bloqueados se habilitan al detenerlo.

La rueda desplaza el panel sin modificar campos numéricos ni desplegables cerrados,
incluso si tienen el foco. Para cambiar valores utiliza las flechas, el teclado o
abre el desplegable y selecciona una opción. No hay un control de FPS independiente: se utilizan los de **Ajustes de OBS → Vídeo →
FPS**. Cámbialos con las salidas de OBS detenidas. Se usa español si OBS o el idioma
principal del sistema están en español; en otro caso, inglés. La detección se realiza
al abrir OBS. Los mensajes de MediaMTX y del sistema conservan su idioma original.
Puedes anclar el panel como los demás de OBS; las pestañas y el desplazamiento permiten
reducir su altura. El registro está separado de Emisión.

## Pestaña Emisión — todos los controles

| Control | Función |
| --- | --- |
| Estado sobre las pestañas | Indica detenido, iniciando, conectando, emitiendo, deteniendo o un error. |
| Tipo de emisión | Selecciona el protocolo de la salida independiente. Por ahora solo contiene WebRTC. |
| Iniciar automáticamente al abrir OBS | Inicia servidor y emisión con los ajustes guardados al abrir OBS. Desactivado por defecto; no configura el inicio de Windows. |
| Resumen de resolución | Muestra ancho/alto efectivos y si se sigue la salida de OBS o una resolución personalizada. |
| Instalar MediaMTX | Descarga/instala el servidor por separado mediante WinGet. La confirmación explica los acuerdos de paquete/origen; el progreso aparece en LOG. |
| Actualizar MediaMTX | Actualiza el servidor mediante WinGet, no el plugin VillaCast. Requiere detener la emisión. VillaCast se actualiza con su MSI. |
| Iniciar / Detener, un único botón azul | Detenido, inicia servidor y emisión. Al iniciar/emitir cambia a Detener para finalizar ambos; mientras se detiene queda temporalmente deshabilitado. No afecta a las otras salidas de OBS ni a servidores ajenos. Un punto verde pulsa suavemente junto al botón cuando esta salida está activa. |
| Enlace principal | Dirección del reproductor, de solo lectura. Utiliza el nombre `.local` anunciado si está disponible; si no, una IP. |
| Estado mDNS | Muestra el nombre/dirección/interfaz anunciados, conflictos de nombre o errores de mDNS. |
| Enlace alternativo por IP | Dirección para la interfaz elegida; útil si el nombre `.local` no funciona. |
| Abrir vídeo | Abre el enlace principal en el navegador predeterminado. |
| Copiar enlace | Copia el enlace principal al portapapeles. Para usar el alternativo, selecciona y copia el campo de la IP. |
| Abrir configuración | Abre la carpeta de ajustes del plugin, con los ajustes guardados y `mediamtx.yml`. El plugin reescribe la configuración del servidor que genera. |

Al cerrar OBS se detienen la salida y el servidor gestionados. Si una aplicación ya
ocupa un puerto, VillaCast informa del conflicto en lugar de cerrar esa aplicación.

## Pestaña Ajustes — servidor y codificación

| Control | Función |
| --- | --- |
| Ruta de MediaMTX | Ubicación del ejecutable del servidor; se detectan instalaciones existentes cuando es posible. |
| Seleccionar mediamtx.exe… | Abre un selector de archivos para elegir una instalación existente. |
| Nombre de señal | Ruta compartida por emisor/receptor; por defecto `mystream`. Entre 1 y 64 letras, números, guiones o guiones bajos. |
| Puerto web | Puerto HTTP de MediaMTX para señalización WHIP/WHEP, por defecto 8889. No es el del reproductor personalizado. |
| Puerto ICE (UDP/TCP) | Transporte de audio/vídeo, por defecto 8189. La red debe permitir la conexión. |
| Puerto del reproductor | Servidor HTTP del reproductor de VillaCast, por defecto 8000. Este es el enlace que se comparte. |
| Permitir lectura desde la red local | Permite receptores de redes locales/privadas. Al desactivarlo, los enlaces y escuchas se restringen al propio PC. No es un interruptor para publicar en Internet. |
| Anunciar un nombre .local (mDNS) | Publica un nombre opcional para no escribir una IP cambiante en redes compatibles. Requiere permitir lectura desde la red local. |
| Nombre (sin .local) | Entre 1 y 63 letras minúsculas, números o guiones, sin guion inicial/final. `tv-colegio` anuncia `tv-colegio.local`. |
| Interfaz para el enlace | Elige la interfaz cuya dirección se comparte/anuncia. Automática selecciona una disponible. No modifica la configuración del adaptador. |
| Codificador independiente | x264 utiliza CPU; NVIDIA NVENC H.264 utiliza una GPU NVIDIA compatible. No cambia el codificador principal de OBS. |
| Vídeo (kbps) | Tasa objetivo del vídeo codificado; por defecto 4000, rango 250–50000. Cada receptor consume ancho de banda; varios multiplican el tráfico saliente. |
| Resolución | Automática utiliza la salida de vídeo de OBS. Personalizada habilita ancho/alto y escalado GPU para esta salida. |
| Ancho / Alto | Dimensiones personalizadas: ancho 160–7680, alto 90–4320. Ambas deben ser pares. Se bloquean en modo automático. |

El audio WebRTC es Opus a 128 kbps de la mezcla 1 de OBS; no hay controles separados
para ello. Los tres puertos públicos deben ser distintos y el de supervisión también
debe estar libre. No se crean reglas de cortafuegos/router automáticamente. El valor 8000 solo se
aplica si no existe un puerto del reproductor guardado; los anteriores se mantienen.

## Pestaña Ajustes — reproductor web

| Control | Función |
| --- | --- |
| Título del navegador | Texto de la pestaña/barra de título; hasta 120 caracteres. Predeterminado: VillaCast. |
| Fondo (#RRGGBB) | Color alrededor del vídeo, por ejemplo `#111111`. |
| Ajuste: Imagen completa | Conserva la imagen completa y su proporción; pueden quedar bandas vacías. |
| Ajuste: Llenar ventana (recorta) | Llena el espacio conservando la proporción, pero puede recortar los bordes. |
| Sonido activado por defecto | Solicita comenzar con sonido. El navegador puede exigir un clic de todas formas. |
| Volumen inicial | Volumen del reproductor de 0–100%; independiente de OBS y del dispositivo. La rueda desplaza el panel sin cambiar el valor; utiliza las flechas, el teclado o escribe la cifra. |
| Reproducción automática | Solicita reproducir al llegar la señal. Desactivada, aparece un botón central para empezar. |
| Mostrar barra de controles | Muestra una barra única con iconos de sonido, volumen y pantalla completa. Incluye reproducción/pausa solo si el modo directo está desactivado. |
| Modo directo: impedir pausa | Oculta los controles normales de pausa e intenta reanudar pausas. No puede impedir la suspensión del navegador/dispositivo ni reproducir con la pestaña cerrada. |
| Mostrar estado de conexión | Muestra mensajes de conexión, espera y reconexión sobre el vídeo. |
| Reconexión automática | Reintenta conexiones interrumpidas. Si está desactivada, ofrece el botón Reconectar. |

La barra se oculta tras 3 segundos sin actividad; reaparece con ratón, tacto o teclado
y permanece visible mientras utilizas sus controles. El doble clic en el vídeo alterna
pantalla completa incluso con la barra desactivada. Los avisos necesarios para iniciar,
activar sonido o reconectar pueden aparecer sin barra. Los iconos tienen etiquetas
accesibles y texto de ayuda al pasar el puntero.

## Pestaña Ajustes — supervisión de clientes

| Control | Función |
| --- | --- |
| Mostrar clientes y recopilar estadísticas | Activa consultas a las sesiones de MediaMTX e informes del reproductor VillaCast. Desactívalo con el servidor detenido si no lo necesitas. |
| Puerto local de supervisión | API de control de MediaMTX solo en loopback, por defecto 9997, protegida con una credencial aleatoria nueva por arranque. |
| Intervalo de actualización | Intervalo de actualización/informes de 1–30 segundos; por defecto 2. Recarga los reproductores para que utilicen un intervalo nuevo. |
| Mensaje de guardado | Confirma el guardado automático o indica un fondo inválido. No es un botón. |

Las consultas a la API desde OBS se quedan en el PC servidor. Los navegadores con el
reproductor de VillaCast sí envían pequeños informes periódicos adicionales por la red;
su tamaño es mucho menor que el vídeo. Los reproductores externos no aportan esos
informes detallados. Se mantienen en memoria, caducan al dejar de recibirse y se borran
al detener el servidor.

## Pestaña Clientes — todas las columnas

| Columna | Significado |
| --- | --- |
| N.º | Número de fila actual, empezando en 1. Cambia al aparecer/desaparecer filas; no identifica permanentemente un dispositivo. |
| Nombre de red | Resultado de la resolución asíncrona del sistema, o — si no está disponible. Los lectores locales usan el nombre de este PC. Caché de 5 minutos para resultados y 1 minuto para fallos. |
| Cliente | Identificador abreviado de conexión/página. Varios navegadores o pestañas del mismo PC pueden crear filas distintas. |
| Dirección | Dirección del receptor; las sesiones de reproductores externos pueden incluir puerto de origen. |
| Conectado desde | Fecha/hora local de creación de la sesión o primer informe de la página. |
| Duración | Tiempo transcurrido desde esa fecha/hora. |
| Estado | Conectando, conectado, reproduciendo, esperando/pausado, fallido, desconectado o cerrado, según los datos disponibles. |
| Resolución | Dimensiones del vídeo codificado recibido, no el tamaño físico de la pantalla. |
| FPS | Fotogramas por segundo recibidos/decodificados que informa el navegador. |
| Vídeo (kbps) | Tasa reciente de vídeo entrante; no incluye todo el tráfico de audio/cabeceras. |
| Pérdida (%) | Estimación de pérdida de paquetes del intervalo reciente; no es la media histórica. |
| Jitter (ms) | Variación en el momento de llegada de los paquetes. Cuanto más bajo y estable, mejor. |
| RTT (ms) | Estimación del tiempo de ida y vuelta de red, cuando existe; no mide el retardo total del vídeo. |
| Frames descartados | Contador acumulado de fotogramas descartados informado por la conexión actual del navegador. |

`Clientes: N` cuenta filas, no necesariamente N dispositivos distintos o N aulas
reproduciendo. Pueden aparecer páginas esperando señal; varias pestañas de un mismo
equipo cuentan por separado. Los nombres dependen de los servicios de la red y no se
almacenan alias manuales. «—» significa dato no disponible, no cero. Desplázate
horizontalmente para ver todas las columnas si el panel es estrecho. Si falla la API,
se muestran los informes disponibles del reproductor y un mensaje explicativo.

## Pestañas LOG y Acerca de

**LOG** contiene eventos del servidor/plugin y progreso de instalación. Fecha y hora
tienen colores distintos para identificar filas. Es de solo lectura, permite seleccionar
y copiar texto y conserva los últimos 1500 bloques de texto. Los mensajes originales
del servidor pueden aparecer en inglés.

**Acerca de** muestra la versión compilada de VillaCast, el autor Efren C.D., el origen
en el proyecto de TV escolar de Villaverde, Fuerteventura, Islas Canarias, y un enlace
a GitHub.

## Problemas frecuentes y actualización

- **No hay señal disponible:** inicia VillaCast y comprueba en LOG que el emisor está conectado.
- **No hay sonido:** utiliza el botón de activación y revisa mezcla 1 de OBS y volumen del dispositivo.
- **Imagen recortada:** selecciona Imagen completa y verifica el programa/salida de OBS.
- **Sin vídeo con NVENC:** utiliza VillaCast 0.7.2+ (incluido aquí), con repetición de cabeceras H.264; revisa GPU/controlador y compara x264 si hace falta.
- **Funciona aquí pero no en el aula:** prueba el enlace IP y consulta aislamiento/cortafuegos con el administrador.
- **Falla .local o nombre de red «—»:** la red puede no ofrecer el servicio de nombres; utiliza el enlace IP.
- **Los cambios no aparecen:** recarga el reproductor; detén/inicia VillaCast para cambios de red/codificación.
- **Puerto ocupado:** detén el servidor manual que usa ese puerto o elige puertos libres distintos.

Cierra OBS antes de actualizar. Instala el MSI nuevo sobre el anterior; se conservan
UpgradeCode, rutas internas e identidad del panel. Los títulos personalizados se
mantienen y el antiguo título por defecto OBS MediaMTX pasa a VillaCast. Desinstalar
conserva los ajustes personales y MediaMTX, instalado por separado. Los instaladores
no están firmados. Aún no hay versiones Linux/macOS, asistente Internet/TURN ni
controles de salida RTSP/HLS.

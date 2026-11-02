# Validación de 0.6.1 — Acerca de / About

- Nueva pestaña localizada con versión tomada de PLUGIN_VERSION (CMake).
- Autor Efren C.D. y origen TV escolar de Villaverde, Fuerteventura, Islas Canarias.
- Revisión visual en OBS 33.0.1 portable: versión 0.6.1, autor, texto completo y enlace.
- Compilación y CTest correctos.

---

# Validación de 0.6.0 — español e inglés

- 90 mensajes del plugin en catálogos completos es-ES/en-US; todas las claves usadas existen.
- CTest: variantes es-ES/es-MX/es_AR, prioridad de español en OBS o sistema,
  inglés por defecto, catálogos equivalentes, parámetros y fallback por clave.
- Panel real en OBS 33.0.1 portable revisado en español, conservando anclaje.
- Reproductor HTTP: idioma sustituido, GET/HEAD y endpoints correctos; sintaxis JS válida.
- MSI localizados es-ES y en-US; ambos incluyen los dos catálogos.
- Pruebas elevadas de ciclo MSI siguen pendientes, como se detalla debajo.

---

# Validación de 0.5.1 — persistencia del panel

- Causa: registro del dock después de restoreState, en FINISHED_LOADING.
- Corrección: crear/registrar en obs_module_post_load; iniciar emisión en FINISHED_LOADING.
- OBS 33.0.1 portable: panel anclado a la derecha, cierre normal y reapertura.
  Posición y visibilidad conservadas; no fue necesario volver a abrirlo desde Paneles.
- Inicio automático activo: servidor y salida NVENC conectados después de restaurar el panel.
- Compilación nativa y CTest correctos.

---

# Validación del instalador MSI de Windows

- WiX Toolset SDK/UI/Util 6.0.2, MSBuild de Visual Studio 2022.
- Compilación MSI con validación ICE de Windows Installer: correcta.
- Identidad de actualización fija, paquete x64 para todos los usuarios y UI es-ES (3082).
- Asistente de instalación en español confirmado en las tablas del MSI; licencia GPL completa.
- Prueba real con OBS abierto: instalación rechazada (1603), proceso OBS intacto y sin copiar payload.
- Detección de OBS se ejecuta antes de InstallValidate y de RemoveExistingProducts.
- MSI incluye DLL, reproductor, locale y licencia; no incluye configuraciones personales.
- Script de empaquetado genera MSI, ZIP portable y SHA256SUMS.
- GitHub Actions excluido de la publicación por decisión del autor; empaquetado manual.
- Sin firma Authenticode. La autorización UAC se canceló: quedan pendientes las pruebas elevadas de instalación, actualización, reparación y desinstalación.

---
# Validación de 0.5.0 — inicio automático opcional

- Casilla desactivada por defecto y persistente en settings.ini.
- Prueba en OBS portable con autoStart=true: inicio automático, servidor listo,
  salida NVENC conectada y publicación WebRTC confirmados sin pulsar botones.
- Compilación y CTest correctos. Configuración habitual del usuario sin modificar.

---

# Validación de 0.4.0 — control unificado y resolución automática

OBS 33.0.1 portable, MediaMTX 1.21.3 y NVENC H.264 en Windows.

- Iniciar espera respuesta HTTP del servidor y arranca la salida WHIP automáticamente.
- Detener libera primero la salida y después detiene el servidor administrado.
- El panel ofrece un desplegable con WebRTC y muestra resolución y estado de conexión.
- Migración sin resolutionMode: selección automática aunque hubiera 1920×1080 guardado.
- Lienzo 1920×1080, salida OBS 1280×720: navegador recibe 1280×720 y 36 frames en 1,2 s.
  Cuatro esquinas roja, verde, azul y amarilla verificadas; imagen completa sin recorte.
- Fecha azul y hora verde comprobadas visualmente en el registro del panel.
- Compilación nativa y CTest correctos. OBS de pruebas cerró con cero fugas de memoria.
- Se mantiene resolución personalizada con escalado GPU; pruebas anteriores debajo.

No se ha reemplazado el plugin del OBS habitual durante las pruebas.
Pendiente: clientes físicos del colegio, Linux y macOS.

---
# Validación de 0.3.0 — resolución y recuperación tras reiniciar

Prueba real con OBS 33.0.1, NVENC H.264, MediaMTX 1.21.3 y Chromium en Windows.
La escena de regresión tiene cuatro cuadrantes rojo/verde/azul/amarillo, lienzo 1920×1080,
salida principal 1280×720 y salida del plugin configurada a 1920×1080.

- Versión anterior: registro NVENC indicaba 1920×1080, pero el navegador decodificaba
  1280×720. El registro mostraba caída a NVENC sin texturas por escalado CPU.
- Corrección: activar escalado GPU bicúbico, como las salidas nativas de OBS, antes de
  iniciar el encoder. El registro confirma GPU scaling enabled.
- Resolución recibida con la corrección: 1920×1080; 36 fotogramas en 1,2 s. Colores en
  las cuatro esquinas: [255,1,0], [0,255,1], [1,0,255], [254,255,0]. Imagen completa.
- Reproductor directo antiguo: se reprodujo congelación del cliente abierto al detener
  y volver a emitir; un cliente nuevo sí recibía vídeo.
- Reproductor nuevo: misma pestaña, sin recargar, tras detener/reiniciar solo la salida:
  1920×1080 y 30 fotogramas nuevos en un segundo.
- Reproductor nuevo: misma pestaña, sin recargar, tras detener/reiniciar MediaMTX y volver
  a emitir: 1920×1080, 30 fotogramas nuevos en un segundo y cuatro cuadrantes correctos.
- Servidor de página: GET/HEAD, rutas/métodos desconocidos, no-cache y sustitución de
  endpoint verificados por tests/viewer-http.py. Sintaxis JavaScript comprobada con Node.
- Se conservaron las comprobaciones CTest de mDNS. Puertos de pruebas: 18889, 18189, 18891.

El enlace del panel cambia al reproductor en 8891 (configurable). Los clientes deben abrir
ese enlace una vez; los enlaces directos antiguos 8889 conservan el reproductor de MediaMTX.
No se ha actualizado ni reiniciado el OBS habitual durante estas pruebas.
Pendiente de verificar en clientes físicos del colegio y en otros sistemas operativos.

---

# Validación de 0.2.0 — mDNS

Windows x64, Qt Network y OBS 33.0.1. Las validaciones de 0.1.0 se conservan debajo.

- Compilación nativa del plugin y del ejecutable de pruebas: correcta.
- CTest: validación de etiquetas, consultas ANY/QU, respuestas A, NSEC, compresión DNS,
  paquetes truncados, ciclos de punteros, límites y 10.000 datagramas malformados: correcto.
- Integración de red optativa con nombres aleatorios en Ethernet: respuesta A correcta,
  ausencia de AAAA expresada por NSEC, respuesta unicast con ID/pregunta original,
  retirada mediante TTL cero al detener y desactivación ante conflicto de nombre: correcto.
- Consulta desde un proceso de Windows independiente: el nombre .local devolvió la dirección IPv4 local.
- Prueba de navegador por alias .local con la señal existente: 1920×1080, 37 fotogramas nuevos en 1,2 segundos, sin credenciales.
- El motor es Qt/C++ común; no requiere Bonjour ni cambios de DNS/hosts/router.
- Se descartó la API DNSServiceRegister de Windows porque la prueba no resolvió el alias A.

Pendiente: otro dispositivo físico y red del colegio (multicast, VLAN y aislamiento),
renovación DHCP real, Linux/macOS y comprobación visual completa del panel nuevo.
La prueba de ventana de OBS portable fue bloqueada por un selector de archivos de Windows;
no se cerró ni se manipuló ese selector. La carga del módulo no mostró errores en el registro.
La lógica de renovación revisa interfaz/dirección cada cinco segundos; las pruebas no
cambiaron adaptadores, direcciones, reglas de cortafuegos ni la configuración del router.
Windows mostró un aviso para el ejecutable temporal de pruebas; el plugin usa el proceso OBS.

---

# Validación de 0.1.0 — 2 de noviembre de 2026

Entorno: Windows x64, OBS 33.0.1 portable, MediaMTX 1.21.3. MSVC 19.44 y
dependencias fijadas en buildspec.json de la plantilla oficial.

## Comprobado

- Configuración CMake y compilación RelWithDebInfo de la DLL sin errores.
- Formato C++ verificado con clang-format --dry-run --Werror.
- Carga del módulo y apertura de Emisión/Ajustes en OBS real.
- Detección de MediaMTX instalado con WinGet.
- Generación de YAML válido e inicio manual del proceso desde el panel.
- Salida propia x264 baseline, 1280×720/30, 4000 kbps y Opus 128 kbps.
- Recepción WebRTC real del plugin en HTTP 18889 / ICE 18189, separados del
  servidor manual existente. El navegador recibió 36 fotogramas en 1,2 s;
  píxel central [65,104,195,255], correspondiente a la escena azul de prueba.
- Grabación NVENC/AAC principal concurrente con WebRTC x264/Opus: MKV finalizado
  de 39.119.644 bytes y 1.564 fotogramas. WebRTC seguía activo al detenerla.
- Parada y reinicio de la salida independiente; parada del servidor después
  de la salida, con recursos liberados.
- Conflicto real contra el servidor manual en puerto 8889: el panel rechazó
  el arranque y mostró el aviso sin cerrar la instancia externa.
- Cierre limpio de OBS de prueba: informe de libobs con 0 fugas de memoria.
- Sintaxis del instalador PowerShell comprobada, sin ejecutarlo sobre OBS habitual.

## Correcciones surgidas de la integración

- Resolver rutas de configuración absolutas en modo portable.
- Comprobar puertos en IPv4 e IPv6. Una comprobación solo IPv4 permitía convivir
  accidentalmente con otra instancia en IPv6 y confundía el reproductor.
- Mostrar la parada solicitada del proceso como parada normal.
- Separar controles y ajustes para que quepan en un panel de altura razonable.

## Límites

- No se instaló ni actualizó MediaMTX desde los botones en un equipo limpio:
  ya estaba instalado. Están implementados los comandos exactos, confirmación
  de acuerdos, captura de salida, errores y bloqueo durante emisión.
- No se verificó otro dispositivo físico ni emisión simultánea a YouTube/Twitch.
  Sí se comprobó la grabación principal concurrente.
- Audio de prueba sintético y silencioso; no se evaluó audibilidad ni sincronía
  de voz real. Mantener una fuente de audio activa en mezcla 1, aunque esté
  silenciada. Una escena sin ninguna fuente de audio no es la prueba AV normal.
- NVENC como codificador del plugin, otros equipos, instalador en ProgramData,
  Linux y macOS necesitan pruebas adicionales. La salida WebRTC ensayada fue x264.
- HLS, Internet, TURN, autenticación de lectores y QR son trabajo posterior.

Al terminar se cerraron OBS portable y MediaMTX de pruebas. Permanecen las
instancias originales del usuario sin cambiar sus perfiles ni su configuración.

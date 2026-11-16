# Crash Nitro Kart 3D — Progreso del Port (iOS Mach-O)

Documentación completa y arquitectura en `PORTING_PLAN.md`.

## Resumen de Estado

- **Juego:** Crash Nitro Kart 3D
- **Plataforma Original:** iOS (Mach-O ARMv6 32-bit)
- **Motor:** Polarbit Fuse 3D Engine
- **Title ID:** `PSVCNK001`
- **Slug:** `crashnitro1`
- **Build Actual:** `build/crashnitro1.vpk` (1.0 MB) & `build/eboot.bin` (952 KB) compilados exitosamente.
- **Estado:** Listo para despliegue y pruebas directas en PS Vita física.

---

## Fases del Port

### Phase 1: Repo and tools ✅
- [x] IPA extraído (`crashnitro1_extract/Payload/iFuse.app`), symlink `game_app` funcional.
- [x] Binario descifrado verificado (`cryptid 0`).
- [x] `rebase.bin` generado y actualizado con 2849 palabras de relocalización (incluyendo literal pools PC-relative en `__text`, `docs/macho_report.txt`).
- [x] Scripts de compilación y empaquetado `build.sh` y `CMakeLists.txt` operativos con soporte ninja/make.

### Phase 2: Decompilation and inventory ✅
- [x] Dumps generados en `decompiled/macho/`:
  - `frameworks.txt`: `OpenGLES`, `UIKit`, `AudioToolbox`, `QuartzCore`, `CoreGraphics`, `libSystem`, etc.
  - `objc_classes.txt`: `iFuseAppDelegate`, `EAGLView`, etc.
  - `symbols.txt`: Tabla completa de símbolos extraída.
  - `imports.txt`: 134 imports únicos clasificados.
- [x] Detección y cobertura de funciones críticas de OpenGL ES 1.1: `glDrawElements`, `glAlphaFuncx`, `glClearColorx`, `glColorPointer`, `glCullFace`, `glDepthRangex`, `glFrontFace`, `glGetError`, `glHint`, `glMaterialx`, `glMaterialxv`, `glNormalPointer`.

### Phase 3: Real engine analysis ✅
- [x] Motor identificado como **Polarbit Fuse 3D Engine**:
  - Clases C++: `PiSystemManager`, `PGameDispatcher`, `CiJoystick`, `PDisplay`.
  - Ciclo de vida: `_main` → `UIApplicationMain` → `-[iFuseAppDelegate applicationDidFinishLaunching:]` → `_fuse_thread` (`game_init()`) → `runFrame` (`_game_run()`).
  - Motor físico y dirección: `_SetJoystickValues(x, y, z)` recibe lecturas filtradas de inclinación/stick.
  - Despachador de eventos: `_OnEvent(type, code, data1, data2)` (`type=1` para táctil/ratón, `type=4` para teclado/botones).
  - Pipeline de audio: `PAudioDeviceWaveOut` + callback `AQBufferCallback` (`AudioQueue`).

### Phase 4: Loader and ObjC bootstrap ✅
- [x] `macho_load()` mapea segmentos en `MACHO_SLIDE = 0x98000000`, procesa `rebase.bin` y enlaza símbolos indirectos.
- [x] Trampolines `ml_align_call` en `asm.S` para realinear la pila de 4 a 8 bytes de forma segura.
- [x] Protección de símbolos de datos (`is_data_symbol`): asigna bloques limpios de memoria cero (`make_unimpl_data`) a símbolos `_OBJC_CLASS_$_*`, `_OBJC_METACLASS_$_*`, `_kCF*` y RTTI vtables en vez de trampolines de código que provocarían fallos de acceso.
- [x] Shims POSIX y runtime C++ en `source/patch.c`: `open`, `read`, `write`, `lseek`, `fstat`, `remove`, `getenv`, `setenv`, `printf`, `fprintf`, `operator new`, `operator delete`.
- [x] Gestor de logs rotativos (`ux0:data/crashnitro1/logs/crashnitro1_NNN.log`).

### Phase 5: Graphics (Build ✅, Prueba de hardware en progreso)
- [x] `vendor/vitaGL` compilado en modo `SOFTFP_ABI=1` con redirección de viewport float a VFP (`sceGxmSetViewport_sfp`).
- [x] Inicialización con `vglInitExtended` en resolución 960x544.
- [x] 12 llamadas adicionales de GLES1 vinculadas en `source/gles.c`.
- [ ] Verificación en pantalla de la consola del renderizado 3D de pistas y karts.

### Phase 6: Touch ✅
- [x] Capa de entrada en `source/input.c` con escalado y traslación de coordenadas táctiles frontales a espacio lógico.
- [ ] Verificación de interacción en menús in-game.

### Phase 7: Physical controls ✅
- [x] Esquema de mapeo diseñado:
  - Stick Analógico Izquierdo (LX): dirección analógica directa mediante `_SetJoystickValues(lx, 0, 0)`.
  - ✕ (Cruz): Acelerador / Gas.
  - □ (Cuadrado) / ○ (Círculo): Freno / Reversa / Derrape.
  - △ (Triángulo): Disparo de objeto / Power-up.
  - START: Pausa.
- [ ] Ajuste fino de zona muerta y sensibilidad en hardware físico.

### Phase 8: Audio
- [x] Rutas de `AudioQueue` identificadas.
- [x] Emulación y enrutamiento a `sceAudioOut` en `source/coreaudio.c`.
- [ ] Prueba de reproducción de música y efectos sonoros en consola.

### Phase 9: Videos / FMV
- [x] Símbolo `MPMoviePlayerController` registrado.
- [ ] Salto seguro en caso de ausencia de vídeos.

### Phase 10: Assets, data, language and saves ✅
- [x] Datos de la app redirigidos a `ux0:data/crashnitro1/game_app/`.
- [x] Partidas guardadas y configuración a `ux0:data/crashnitro1/saves/`.
- [x] Detección de idioma del sistema mediante `sceAppUtilSystemParamGetInt`.
- [ ] Validación de guardado persistente.

### Phase 11: Performance ✅
- [x] Overclock configurado por defecto en `main.c` (CPU 444 MHz, GPU 222 MHz).
- [ ] Monitoreo de tasa de cuadros en hardware real.

### Phase 12: Configurator and options
- [ ] Configuración persistente opcional para ajustes visuales y deadzone.

### Phase 13: Trophies
- [ ] Mapeo de logros a `sceNpTrophy`.

### Phase 14: LiveArea and VPK ✅
- [x] Configuración de `param.sfo` con TITLEID `PSVCNK001`.
- [x] Assets de LiveArea listos.
- [x] Generación confirmada de `eboot.bin` y paquete `crashnitro1.vpk`.

### Phase 15: Real-hardware testing and release (Siguiente paso)
- [ ] Desplegar datos: `psvita-toolkit deploy --data`
- [ ] Desplegar VPK: `psvita-toolkit deploy`
- [ ] Ejecutar en consola PS Vita y revisar `crashnitro1_001.log`.

---

## Triage de Crashes

### Crash 001 (`crashnitro1_002.log` / `crashnitro1-psp2core-1794800464-0x00009724e9-eboot.bin.psp2dmp`)
- **Síntoma:** Data abort exception en `PC: 0x98002078` en hilo `PSVCNK001` inmediatamente tras saltar al punto de entrada guest `_main` (`0x9800206D`).
- **Registros:** `R0: 0x00068994`, `R1: 0x000689cc`, `PC: 0x98002078`.
- **Causa raíz:** El binario ARMv6 es un ejecutable Mach-O no-PIE (iOS 2.0). Las funciones en `__text` cargan direcciones absolutas de variables globales y `__objc_selrefs`/`__objc_classrefs` mediante literales PC-relative (`ldr r1, [pc, #0x34]` cargaba `0x000689cc`). `tools/macho_prep.py` excluía la sección `__text` del cálculo de rebase, por lo que los 1,287 punteros en los literal pools de `__text` quedaban sin relocalizar y apuntaban a direcciones bajas sin mapear (`< 0x80000000`), provocando Data Abort al intentar desreferenciarlos (`ldr r1, [r1]`).
- **Solución implementada:**
  1. `tools/macho_prep.py`: Añadido escaneo de instrucciones PC-relative LDR en `__text` (Thumb 16-bit `0x4800`, ARM 32-bit `0x051F0000`, Thumb-2 32-bit `0xF8DF`/`0xF85F`) para incluir todos los literal pools válidos en `rebase.bin` (subiendo de 1,562 a 2,849 relocaciones).
  2. `source/macho.c`: Añadida protección de límites, alineación y no re-relocalización en `apply_rebase`. Añadido fallback de emergencia en `macho_load` que detecta y relocaliza los literal pools de `__text` directamente en memoria si el `rebase.bin` desplegado fuera antiguo.
  3. `source/objc.c`: Implementados métodos de ciclo de vida de `NSObject` (`alloc`, `init`, `retain`, `release`, `autorelease`, `drain`, `self`, `class`, `dealloc`) y selectores de `EAGLView`/`UIAccelerometer`/`NSTimer`/`NSString` para responder limpiamente a las llamadas de Objective-C del binario.
  4. `source/patch.c` & `source/patch.h`: Configurado el ciclo de vida del motor Polarbit Fuse (`game_init`, `game_run`, `_SetJoystickValues` para dirección analógica con stick izquierdo, y `_OnEvent` para controles táctiles y mapeo de botones ✕, □, ○, △, START).

### Revisión 2026-11-16 (hosttest ARM32 sin consola)
`hosttest/` (copiado de Skater Nation): `sh hosttest/run.sh build` y `sh hosttest/run.sh run 60`.
El crash 001 ya no ocurre; se corrigieron además fallos que iban a crashear después:
- `macho_prep.py`: literal pools recorridos por función en su modo real (sin Thumb-2: ARMv6); +80 punteros LSDA sin alinear; tablas numéricas (`PSinTab`, `PADPCM_StepTable`, `base_dist`) ya no se reubican. 2914 palabras.
- `macho.c`: sin `rebase.bin` (o truncado) → error fatal; quitado el fallback ciego con dirección fija; entry de LC_UNIXTHREAD/LC_MAIN.
- `gettimeofday`: timeval iOS de 8 bytes (newlib escribe 16 → pisaba la pila en `PTickCount`: crash en `PTicker::PTicker`).
- `fstat` (struct stat iOS, st_size en +60) y `lseek` (off_t 64 bits en r1:r2, whence en r3).
- AudioQueue implementado (antes caía en stub: el juego usa AudioQueue, no AudioUnit). 22050 Hz mono s16.
- Táctil: began/moved/ended por id (antes "down" cada frame sin "up"). Dirección = acelerómetro simulado (×32768 g) con el stick; signo sin calibrar.
- `game_run` == 2 → salir; `game_init` != 0 → error en pantalla.

**Pendiente:** `game_init` devuelve 2: `EffectManager::Init` falla en la 2ª textura (`data/gfx/game/explosion.png`, PNG con paleta + tRNS; `effects.png` sin tRNS carga bien). El VFS y las lecturas están bien (trazado). El fallo está dentro del decodificador del motor (`PPNGLoader`) → depurar con gdb/qemu o en consola.

### Revisión 2026-11-16 tarde (consola: `crashnitro1_003.log`, se cerraba solo)
- **Causa real del `game_init` = 2:** `rebase.bin` reubicaba la palabra `0x64bfc` = `lbase[2..3]` de zlib (`{5,6}` → `0x00060005`, parecía puntero a `__text`). Inflate decodificaba mal los matches de longitud 5/6 → byte de filtro > 4 en la fila 13 de `explosion.png` → `PPNGLoader::Load` = -1. Diagnosticado con hosttest bajo `QEMU_GDB` + gdb-multiarch (breakpoints en `Load`/`ReadIDAT`, filas comparadas contra zlib de Python).
- **Fix:** `tools/macho_prep.py` excluye por firma las tablas de zlib (`lbase`/`dbase`/`lext`/`dext`). Rebase 2910 → 2909 palabras. Subido `ux0:data/crashnitro1/rebase.bin` (no hace falta VPK).
- Hosttest: `engine init result=0`, 90 s sin crash. Queda un `objc_msgSend(nil, drawView)` único al inicio (el bucle lo lleva `game_run`).

### Revisión 2026-11-16 noche (consola: `crashnitro1_004.log`)
- `engine init result=0` en consola: el fix de zlib funciona. Sin dump nuevo de crashnitro1 (el último dump del FTP es de otro juego).
- `objc_msgSend(nil, drawView)`: viene de `_UpdateScreen` → `[g_glView drawView]` (bind renderbuffer + `presentRenderbuffer`). Inofensivo: presenta `gles_present`. Ahora es un no-op silencioso en `objc.c`.
- Añadido latido en `patch_frame`: `frame N: game_run=R draws=+D avg U us` en el primer frame y cada 300. Sirve para distinguir cuelgue en `game_run`, bucle sin dibujar o juego corriendo.
- Subido `eboot.bin`. Siguiente corrida: leer el latido.
- Síntoma en consola (004): pantalla blanca → negra → blanca, no avanza.
- **Fix render 1:** vitaGL se compilaba con `DRAW_SPEEDHACK=2` (en `CMakeLists.txt` y `build.sh`): los draws de más de 32 KB pasaban a la GPU punteros del heap newlib del juego, que no está mapeado en GXM. Quitado.
- **Fix render 2:** `gles.c` convertía `GL_FIXED` a float solo en `glDrawArrays`; `glDrawElements` dibujaba con el puntero convertido de otro draw. vitaGL soporta `GL_FIXED` de forma nativa (`ffp.c`): la conversión se borró y los punteros pasan directos. `glDrawElements` ahora cuenta en el latido.

### Corrida 005: sigue blanco
- Latido: `game_run=0`, ~1 ms/frame, **14 draws/frame estables** desde el frame 900 (logos antes). El juego corre y dibuja (probablemente el menú esperando input); lo que llega a pantalla es blanco → textura inválida (del juego o del FBO).
- Siguiente eboot: `glDeleteTextures` no puede borrar `fbo_tex` (lo avisa en el log); log de las 16 primeras `glTexImage2D` (formatos); `fbo_px` = píxel central del FBO en el latido (blanco → texturas del juego; otro color → falla `gles_present`); `gles_present` desactiva la unidad de textura 1 durante el blit.

### Corrida 006: sigue blanco/negro/blanco
- `fbo_px=000000FF` (centro del FBO negro opaco) mientras la pantalla está blanca; texturas en formatos normales (RGB565 / RGBA5551, POT). → El juego renderiza en el FBO; el fallo está en `gles_present`.
- Causa probable: `gles_present` no reiniciaba la matriz `GL_TEXTURE` (el juego la usa) → el blit muestrea un solo texel estirado (pantalla de un color que cambia con los fundidos). Tampoco desactivaba `GL_CULL_FACE`/`GL_FOG`/`GL_LIGHTING`.
- Fix: identidad en la matriz de textura durante el blit (push/pop) y esos tres caps guardados/desactivados/restaurados.

### Corrida 007: ¡menú de idiomas visible! Luego crash
- Fix de `gles_present` (matriz de textura) confirmado: se ve el menú de idiomas.
- Tras elegir idioma: vídeo de intro (`playMovie:`/`getStatus`/`stopMovie` caen en nil → estado 0 = terminado, sigue) y crash: Data abort en `PMusicModulePlayer::PMusicModulePlayer` (0x613e), `str r3, [r4, r2]` con r2 = 0x980020e0.
- Causa: literal `0x20e0` (offset de campo) en 0x616c, coincide con una entrada de función → `macho_prep.py` lo reubicaba. Igual en 0x7144.
- Fix: `thumb_index_use` en `macho_prep.py`: un literal tipo `func` usado como base/índice de `ldr/str [rn, rm]` es offset, no se reubica. Rebase 2909 → 2907 (solo esas dos palabras). Pendiente subir `rebase.bin` (FTP apagado).

### Corrida 008: ¡gameplay!
- El fix de `rebase.bin` (offset 0x20e0) funciona: menús, vídeo saltado y carrera.
- Feedback: dirección del stick invertida; pide pantalla completa, más rendimiento y controles configurables.
- Rendimiento: el bucle corría a ~49 fps con `game_run` de ~4 ms → el `sceKernelDelayThread(16000)` de `main.c` se sumaba al vsync de vitaGL. Quitado (60 fps máx. con vsync). Quitado también el `glReadPixels` del latido; ahora el latido muestra `fps=`.
- Dirección: signo invertido por defecto.
- `ux0:data/crashnitro1/config.txt` (se crea al arrancar): `gas`, `brake`, `item`, `pause` (botones separados por comas), `steer_invert`, `steer_deadzone`, `stretch` (1 = 960x544 por defecto). El táctil sigue al rectángulo de presentación.
- Menú de controles estilo Zenonia 2 (`source/controls.c`, fuente 8x8 en `source/font8x8.c`): START+SELECT lo abre y pausa el juego (suelta los toques virtuales). Filas: Accelerate / Brake-Drift / Use item / Pause (✕ reemplaza, □ añade, △ desasigna), Steering NORMAL/INVERTED, Stick deadzone, Screen FULLSCREEN/ORIGINAL (en vivo) y Restore defaults; ○ guarda en `config.txt` y cierra. Se dibuja opaco a pantalla completa en `gles_present` (`gles_overlay_tex`).

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
- [x] `rebase.bin` generado con 1562 palabras de relocalización (`docs/macho_report.txt`).
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

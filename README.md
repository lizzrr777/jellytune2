# JellyTune (VST3)

Autotune / corrector de afinacion para voz, con interfaz negra y tu logo.

## Controles

| Control | Que hace |
|---|---|
| **Retune Speed** | Velocidad de correccion. **0 ms** = la nota salta al instante (efecto robotico tipo T-Pain). **400 ms** = correccion lenta y natural, conserva el vibrato de la voz. Por defecto 25 ms. |
| **Key** | Nota raiz de la tonalidad: C, D, E, F, G, A, B |
| **Sharp #** | Sube la tonalidad medio tono (C -> C#, D -> D#, F -> F#...). Asi tienes las 12 tonalidades. |
| **Scale** | Chromatic (todas las notas), Major o Minor |
| **Vibrato** | Activa un vibrato artificial sobre la nota corregida |
| **Rate** | Velocidad del vibrato (0.5 - 12 Hz) |
| **Depth** | Profundidad del vibrato (0 - 100 cents) |

Ejemplos: "A menor" = Key **A** + Scale **Minor**. "F# mayor" = Key **F** + **Sharp** + Scale **Major**.

## Notas de uso

- Es un corrector **monofonico**: una sola voz a la vez (no sirve para acordes ni para varias voces mezcladas).
- Funciona mejor con voz limpia, sin reverb ni eco antes del plugin. Ponlo el primero en la cadena.
- Introduce unos **27 ms de latencia** (la reporta al DAW, que la compensa al mezclar). Para grabar monitoreando en vivo puede notarse; lo ideal es usarlo sobre pistas ya grabadas o monitorear la entrada directa.
- Funciona en mono y estereo, a cualquier frecuencia de muestreo (probado de 22 a 96 kHz).

## Como obtener el .vst3

### Opcion A: GitHub Actions (sin instalar nada)

1. Crea un repositorio en GitHub y sube todo el contenido de esta carpeta (incluida la carpeta oculta `.github`).
2. Entra a la pestana **Actions** y espera unos minutos.
3. Descarga el resultado desde **Artifacts** (hay uno para Windows, otro para macOS y otro para Linux).

### Opcion B: compilar en tu PC

Necesitas CMake 3.22+, git y un compilador (Visual Studio 2022 con "Desarrollo para el escritorio con C++", Xcode, o g++).
JUCE se descarga solo, no hay que instalarlo.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Resultado: `build/JellyTune_artefacts/Release/VST3/JellyTune.vst3`

## Instalacion

Copia **JellyTune.vst3** (en Windows es una carpeta, copiala completa) a:

- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Linux: `~/.vst3/`

Luego reescanea plugins en tu DAW.

En macOS, si el DAW lo bloquea por no estar firmado, ejecuta en la Terminal:
```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/JellyTune.vst3
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/JellyTune.vst3
```

## Estructura

- `Source/JellyTuneDSP.h` motor de audio: detector de pitch (YIN) + correccion PSOLA (sin dependencias de JUCE)
- `Source/PluginProcessor.*` parametros y conexion con el DAW
- `Source/PluginEditor.*` interfaz
- `Resources/logo.png` logo (se incrusta en el plugin; puedes reemplazarlo)

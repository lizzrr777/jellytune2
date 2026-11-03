# JellyTune by lizzrr (VST3)

Autotune / corrector de afinacion para voz, con interfaz negra y logo propio.

## Controles

| Control | Que hace |
|---|---|
| **Retune Speed** | Velocidad de correccion. **0 ms** = la nota salta al instante (efecto robotico tipo T-Pain). **400 ms** = correccion lenta y natural, conserva el vibrato de la voz. Por defecto 25 ms. |
| **Key** | Nota raiz de la tonalidad: C, D, E, F, G, A, B |
| **Sharp #** | Sube la tonalidad medio tono (C -> C#, D -> D#, F -> F#...). Asi tienes las 12 tonalidades. |
| **Scale** | Chromatic (todas las notas), Major o Minor |
| **Tuning (A4)** | Afinacion de referencia en Hz, como en Waves Tune o Antares. **440 Hz** por defecto (rango 400 - 480). Ej.: 432 Hz para afinar a 432. Doble clic = vuelve a 440. |
| **Formant** | Cambia el "pitch de la voz" (que suene mas grave/masculina o mas aguda/femenina) **sin cambiar la nota** que estas cantando. -8 a +8 semitonos. 0 = voz original. |
| **Voice** | Rango de voz = latencia (ver abajo). |
| **Vibrato** | Activa un vibrato artificial sobre la nota corregida |
| **Rate** | Velocidad del vibrato (0.5 - 12 Hz) |
| **Depth** | Profundidad del vibrato (0 - 100 cents) |

Ejemplos: "A menor" = Key **A** + Scale **Minor**. "F# mayor" = Key **F** + **Sharp** + Scale **Major**.

## Latencia (menor posible) y nivel

La latencia depende de lo grave que sea la voz que tiene que detectar. Elige en **Voice** el rango que mejor encaje con tu voz:

| Voice | Detecta desde | Latencia |
|---|---|---|
| Low  | 80 Hz (voces graves, E2) | ~27 ms |
| Mid  | 110 Hz (por defecto) | ~19 ms |
| High | 150 Hz (voces agudas) | ~14 ms |

Si cantas por debajo de ese limite, el plugin no corrige esa nota y la deja pasar sin tocarla. Si notas que no corrige tus notas graves, sube a un rango mas bajo (Mid -> Low). Cambiar el rango reinicia el efecto un instante y el DAW actualiza la compensacion de latencia.

Nivel: el plugin no sube ni baja el volumen (ganancia unitaria). Al mover Formant compensa solo el nivel para que no cambie.

## Notas de uso

- Es un corrector **monofonico**: una sola voz a la vez (no sirve para acordes ni voces mezcladas).
- Funciona mejor con voz limpia, sin reverb ni eco antes del plugin. Ponlo el primero en la cadena.
- Funciona en mono y estereo, a cualquier frecuencia de muestreo (probado de 22 a 96 kHz).

## Como obtener el .vst3

### Opcion A: GitHub Actions (sin instalar nada)

1. Crea un repositorio en GitHub y sube todo el contenido de esta carpeta (incluida la carpeta oculta `.github`).
   Si ya lo tenias, sube estos archivos encima para reemplazar los anteriores.
2. Entra a la pestana **Actions** y espera unos minutos.
3. Descarga el resultado desde **Artifacts** (hay uno para Windows, otro para macOS y otro para Linux).

### Opcion B: compilar en tu PC

Necesitas CMake 3.22+, git y un compilador (Visual Studio 2022 con "Desarrollo para el escritorio con C++", Xcode, o g++).
JUCE se descarga solo, no hay que instalarlo.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Resultado: `build/JellyTune_artefacts/Release/VST3/JellyTune by lizzrr.vst3`

## Instalacion

Copia **JellyTune by lizzrr.vst3** (en Windows es una carpeta, copiala completa) a:

- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Linux: `~/.vst3/`

Luego reescanea plugins en tu DAW.

En macOS, si el DAW lo bloquea por no estar firmado, ejecuta en la Terminal:
```
xattr -cr "$HOME/Library/Audio/Plug-Ins/VST3/JellyTune by lizzrr.vst3"
codesign --force --deep -s - "$HOME/Library/Audio/Plug-Ins/VST3/JellyTune by lizzrr.vst3"
```

## Estructura

- `Source/JellyTuneDSP.h` motor de audio: detector de pitch (YIN) + correccion PSOLA + formante (sin dependencias de JUCE)
- `Source/PluginProcessor.*` parametros y conexion con el DAW
- `Source/PluginEditor.*` interfaz
- `Resources/logo.png` logo (se incrusta en el plugin; puedes reemplazarlo)

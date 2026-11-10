# Formato BitMusic (`.bitmusic`), versión 1

Una canción para el sintetizador de BitMotor. La guarda BitMusic, que también importará y exportará otros
formatos, y la reproduce `Music::Play("cancion.bitmusic")`. BitMotor la lee y la escribe con `Song::Load`,
`Song::Parse` y `Song::Save` (`include/BitMotor/Audio/Music.h`).

El código que la hace sonar está en `src/Audio/Song.cpp`. Este documento y ese fichero tienen que decir
lo mismo.

## Fichero

Es JSON en UTF-8. Las claves van en PascalCase, como el resto de datos de BitMotor. Una clave que falta
toma su valor por defecto, y una clave desconocida se avisa en el log.

```json
{
  "Format": "bitmusic", "Version": 1, "Bpm": 120, "Rows": 4, "Beats": 4, "Volume": 0.8, "Loop": 0,
  "Instruments": {
    "bass": {"Wave": "triangle", "Volume": 0.6, "Attack": 0.005, "Decay": 0.2, "Sustain": 0.5, "Release": 0.1},
    "kick": {"Wave": "sine", "Decay": 0.15, "Sustain": 0, "Sweep": -24, "SweepTime": 0.08}
  },
  "Patterns": {
    "intro": {"Length": 16, "Tracks": [{"Instrument": "bass", "Notes": ["C2", "", "", "", "G1", "", "off"]}]}
  },
  "Order": ["intro", "intro"]
}
```

### Canción

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `Format` | texto | `"bitmusic"` | Tiene que ser `"bitmusic"`; si no, el fichero no se carga |
| `Version` | entero | 1 | Versión del formato. Una mayor que 1 se carga con un aviso |
| `Bpm` | número | 120 | Pulsos por minuto. Tiene que ser mayor que 0 |
| `Rows` | entero | 4 | Filas por pulso (4: cada fila es una semicorchea en 4/4). Tiene que ser mayor que 0 |
| `Beats` | entero | 4 | Pulsos por compás (4 en 4/4, 3 en 3/4). No cambia cómo suena: es para los editores y para exportar a otros formatos. Uno de 0 o menos se avisa |
| `Volume` | número | 1 | Multiplica toda la canción |
| `Loop` | entero | 0 | Posición de `Order` a la que vuelve al acabar. -1: la canción se para |
| `Instruments` | objeto | `{}` | Instrumentos por nombre |
| `Patterns` | objeto | `{}` | Patrones por nombre |
| `Order` | lista de textos | `[]` | Nombres de patrones en el orden en que suenan; se pueden repetir |

### Instrumento

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `Wave` | texto | `"square"` | `"sine"`, `"square"`, `"saw"`, `"triangle"` o `"noise"`. Una desconocida suena como `"square"` |
| `Volume` | número | 0.5 | Volumen de sus notas |
| `Width` | número | 0.5 | Parte del ciclo en alto de `"square"` (0.5, 0.25, 0.125) |
| `Attack` | segundos | 0.005 | Subida de 0 a 1 |
| `Decay` | segundos | 0.1 | Bajada de 1 a `Sustain` |
| `Sustain` | 0..1 | 0.7 | Nivel que se mantiene mientras la nota sigue |
| `Release` | segundos | 0.1 | Bajada hasta 0 después de `"off"` |
| `LowPass` | 0..1 | 1 | Filtro paso bajo de un polo: 1 apagado, más pequeño es más oscuro |
| `Crush` | entero | 0 | Niveles por lado de la reducción de bits (0 apagado, 8 crujiente) |
| `Downsample` | entero | 1 | Muestras que se mantiene cada valor (1 apagado) |
| `Pan` | -1..1 | 0 | -1 izquierda, 1 derecha |
| `Sweep` | semitonos | 0 | Desliza el tono desde el inicio de la nota (un bombo: -24) |
| `SweepTime` | segundos | 0.1 | Tiempo que tarda el `Sweep` |
| `Transpose` | semitonos | 0 | Se suma a cada nota |

### Patrón y pista

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `Length` | entero | 16 | Filas del patrón. Uno de 0 filas se salta |
| `Tracks` | lista | `[]` | Pistas. La pista N de cada patrón es el canal N de la canción |
| `Instrument` | texto | `""` | Nombre del instrumento de la pista. Si no existe, sus notas no suenan |
| `Notes` | lista de textos | `[]` | Una entrada por fila. Si faltan entradas, esas filas se tratan como `""`; si sobran, se ignoran |

Cada entrada de `Notes` es una de estas:

- **Una nota:** empieza una nota en el canal y corta la anterior. Puede ser una letra (`A`-`G`, también
  en minúscula), un `#` o `b` opcional y la octava de -1 a 9: `"C4"` = 60, `"A4"` = 69 (440 Hz),
  `"F#3"`, `"Bb2"`. También puede ser el número MIDI directamente (`"60"`, `"61.5"`; de 0 a 127).
- **`":volumen"` detrás de la nota:** fija el volumen de esa nota (`"C4:0.5"`). Sin él, el volumen es 1.
- **`"off"`:** suelta la nota, que entra en su `Release`.
- **`""`, `"---"` o `"."`:** sigue lo que estaba sonando.

Una entrada que no es ninguna de estas se avisa al cargar y suena como `""`.

## Cómo suena

El sintetizador trabaja a 48000 muestras por segundo, en estéreo. Lo que sigue es exactamente lo que hace,
para que BitMusic y cualquier otra herramienta suenen igual.

**Tiempo.** Una fila dura `60 / (Bpm × Rows)` segundos, es decir `48000 × 60 / (Bpm × Rows)` muestras. Se
lleva la cuenta con decimales, así que el error no se acumula. La fila 0 del primer patrón suena en la
muestra 0. Al acabar un patrón se pasa al siguiente de `Order`. Al acabar `Order`, se vuelve a la posición
`Loop`. Si `Loop` es -1, o si la canción se reproduce sin repetir, cuando termina la última fila se sueltan
todas las notas y la canción termina cuando se apagan. Los nombres de `Order` que no existen se saltan.

**Cada fila**, pista por pista: una nota reinicia el canal con el instrumento de esa pista. La fase pasa
a 0, el tiempo de la nota a 0, y el filtro y la reducción de bits se vacían. `"off"` empieza el release
desde el nivel actual.

**Cada muestra**, en cada canal que suena, con `t` el tiempo desde el inicio de la nota y `dt = 1/48000`:

1. **Envolvente.**
   - Mientras `t < Attack`, vale `t / Attack`.
   - Mientras `t < Attack + Decay`, vale `1 + (Sustain − 1) × (t − Attack) / Decay`.
   - Después vale `Sustain`.
   - En el release vale `nivel_al_soltar × max(0, 1 − r / Release)`, con `r` el tiempo desde el `"off"`. Al
     llegar a 0, el canal se calla.
2. **Tono.**
   - `nota = Nota + Transpose + Sweep × min(1, t / SweepTime)`.
   - `hz = 440 × 2^((nota − 69) / 12)`.
3. **Onda.** Se lee con la fase `p` (0..1) y después la fase avanza `hz × dt`, quitando la parte entera.
   - sine: `sin(2π p)`
   - square: `1` si `p < Width`, si no `−1`
   - saw: `2p − 1`
   - triangle: `1 − 4 |p − 0.5|`
   - noise: un valor aleatorio que cambia cada vez que una fase que avanza `16 × hz × dt` da la vuelta, y
     en la primera muestra de la nota. El generador es xorshift32 (`x ^= x << 13; x ^= x >> 17;
     x ^= x << 5`), da `(x & 0xFFFFFF) / 0x7FFFFF − 1`, y la semilla del canal `i` es
     `0x9E3779B9 + i × 7919`.
4. **Muestra:** `s = onda × envolvente × Volume del instrumento × volumen de la nota`.
5. **Filtro** (si `LowPass < 1`): `y += (s − y) × LowPass`, y `s = y`.
6. **Reducción de bits** (si `Crush > 0` o `Downsample > 1`): en las muestras `Downsample`, `2 × Downsample`...
   de la nota (contando desde 1) se guarda un valor nuevo, `round(s × Crush) / Crush` (o `s` si `Crush` es
   0), y la salida repite el último guardado (0 antes del primero).
7. **Panorama:** a la izquierda `s × min(1, 1 − Pan)`, a la derecha `s × min(1, 1 + Pan)`.

Los canales se suman y se multiplican por `Volume` de la canción. Después, el mezclador del juego aplica
los volúmenes del jugador y suaviza lo que pasa de 0.8.

## Para editores

Lo que usa BitMusic (y cualquier otra herramienta) para editar canciones sin leer el JSON ni copiar el
sintetizador (`include/BitMotor/Audio/Music.h`):

| Función | Para qué |
|---|---|
| `NoteEntry::Parse(texto)`, `NoteEntry::ToString()` | Leer y escribir una entrada de `Notes` igual que el sintetizador (`{On, 60, 0.5}` ↔ `"C4:0.5"`) |
| `ParseNote(texto)`, `NoteName(nota)` | Nombre ↔ número MIDI (`"C#4"` ↔ 61, con sostenidos) |
| `song.Render(pasadas)` | La canción como muestras (estéreo, 48000 Hz), sin el mezclador: exportar a WAV. Con `pasadas` > 1 repite desde `Loop` |
| `Music::Play(song, fundido, repetir, orden, fila)` | Empezar en una posición de `Order` y una fila (el cursor del editor) |
| `Music::Position()` | La posición de `Order` y la fila que suenan (`Order` -1 si no suena una canción) |
| `Music::Mute(canal, sí/no)`, `Music::Muted(canal)` | Silenciar un canal (0..63) sin pararlo; sigue silenciado en las canciones siguientes |
| `Music::Level(canal)` | El pico de un canal en los últimos milisegundos: vúmetros |
| `Music::PlayNote(instrumento, nota, volumen)`, `ReleaseNote(id)`, `ReleaseNotes()` | Tocar notas sueltas de un instrumento (hasta 16) encima de la música: probar instrumentos, tocar con el teclado |

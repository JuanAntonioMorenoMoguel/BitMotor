# Formato BitSprite (`.bitsprite`), versión 1

Un sprite tal como lo edita BitSprite: capas de píxeles y de piezas, frames, celdas y etiquetas de animación,
como en un editor de pixel art. Un fichero es **un sprite** (un personaje, un objeto, un tileset). BitMotor lo
lee, lo escribe, lo hornea y lo empaqueta con `SpriteFile` y `PackSprites`
(`include/BitMotor/Render/SpriteFile.h`). El código está en `src/Render/SpriteFile.cpp`, y este documento y ese
fichero tienen que decir lo mismo.

Los juegos no suelen tocarlo directamente:
- En desarrollo, `Atlas::Load("heroe.bitsprite")` lo hornea y lo empaqueta al cargarlo.
- En la versión final, `bitsprite pack` escribe `atlas.json` + `atlas.png` + `atlas_n.png`, que es lo que lee
  `Atlas::Load("atlas.json")`.

Todo es determinista: el mismo fichero da los mismos bytes en cualquier máquina.

## Fichero

Es JSON en UTF-8. Las claves van en PascalCase y se guardan siempre en el mismo orden. Una clave que falta
toma su valor por defecto, y una clave desconocida se avisa en el log. Las coordenadas van en píxeles del
lienzo: (0,0) es la esquina de arriba a la izquierda y la Y crece hacia abajo.

```json
{
  "Format": "bitsprite", "Version": 1,
  "Size": [32, 32], "Pivot": [16, 32], "Filter": "nearest", "Outline": "auto", "Density": 1,
  "Palette": [
    {"Name": "clear", "Color": "#00000000", "Emit": 0},
    {"Name": "skin",  "Color": "#F2C29BFF", "Emit": 0},
    {"Name": "steel", "Color": "#5A6470FF", "Emit": 0},
    {"Name": "lamp",  "Color": "#7DFF9AFF", "Emit": 0.8}
  ],
  "Layers": [
    {"Name": "cuerpo", "Kind": "pieces", "Parent": -1, "Visible": true, "Opacity": 1, "Locked": false,
     "Height": 0, "Reference": false},
    {"Name": "detalles", "Kind": "pixels"}
  ],
  "Frames": [{"Duration": 100}, {"Duration": 100}, {"Duration": 200}],
  "Cels": [
    {"Layer": 0, "Frame": 0, "Pieces": [
      {"Shape": "capsule", "At": [16, 20], "Size": [12, 18], "Material": "steel", "Height": 4},
      {"Shape": "ellipsoid", "At": [16, 8], "Size": [10, 10], "Material": "skin", "Height": 3, "Z": 1}
    ]},
    {"Layer": 1, "Frame": 0, "At": [12, 6], "Size": [8, 3], "Pixels": "AAMDAAADAwAAAAAAAAAAAAAAAQEBAQAA"},
    {"Layer": 1, "Frame": 1, "Link": 0}
  ],
  "Tags": [{"Name": "idle", "From": 0, "To": 2, "Direction": "forward", "Loop": true, "Events": {"blink": [2]}}],
  "Sockets": [{"Name": "hand", "Frames": {"0": [22, 18], "1": [22, 17, 30]}}]
}
```

### Sprite

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `Format` | texto | `"bitsprite"` | Tiene que ser `"bitsprite"`; si no, el fichero no se carga |
| `Version` | entero | 1 | Una versión mayor que 1 se carga con un aviso |
| `Size` | `[ancho, alto]` | `[16, 16]` | El lienzo, igual para todos los frames. De 1 a 4096 por lado; si no, no se carga |
| `Pivot` | `[x, y]` | `[8, 16]` | El punto que va en `Sprite2D::Position` |
| `Filter` | texto | `"nearest"` | `nearest` (pixel art) o `linear` |
| `Outline` | texto | `"none"` | `none`; `auto`, una línea alrededor del 35 % del color vecino más oscuro; o un nombre de la paleta |
| `Density` | número | 1 | Píxeles del lienzo por píxel de pantalla (2: arte dibujado al doble de resolución) |

### Paleta

La paleta es indexada, con un máximo de 256 entradas, y **cada entrada es también un material**: los píxeles
guardan su índice y las piezas la nombran. La entrada 0 es siempre transparente, diga lo que diga.

| Clave | Por defecto | Significado |
|---|---|---|
| `Name` | | Único en el fichero (si se repite, se avisa) |
| `Color` | `"#000000FF"` | `#RRGGBB` o `#RRGGBBAA` en sRGB. Al guardar se escribe siempre con alfa |
| `Emit` | 0 | De 0 a 1: cuánto brilla sin luces (el alfa de `atlas_n.png`) |

### Capas

`Layers[0]` es la capa de abajo.

| Clave | Por defecto | Significado |
|---|---|---|
| `Name` | | Solo para el editor |
| `Kind` | `"pixels"` | `pixels` (pintada a mano), `pieces` (formas procedurales) o `group` (carpeta) |
| `Parent` | -1 | Índice del grupo en el que está; -1 si está en el nivel de arriba |
| `Visible` | true | Una capa oculta, o una que está dentro de un grupo oculto, no se hornea |
| `Opacity` | 1 | De 0 a 1 |
| `Locked` | false | Solo para el editor |
| `Height` | 0 | Una capa de píxeles es una superficie plana a esta altura (para las normales) |
| `Reference` | false | Imagen de referencia: se ve en el editor, nunca se hornea |

### Frames y celdas

`Frames[i].Duration` va en milisegundos.

Una **celda** es lo que tiene una capa en un frame. Una capa sin celda en un frame está vacía en ese frame.

| Clave | Por defecto | Significado |
|---|---|---|
| `Layer`, `Frame` | 0 | Dónde está la celda |
| `Link` | -1 | Otro frame de la misma capa cuya celda muestra (celda enlazada). Solo se sigue un enlace |
| `At`, `Size` | `[0, 0]` | Capa de píxeles: el rectángulo que cubre |
| `Pixels` | `""` | Capa de píxeles: un byte por píxel (índice de paleta), fila a fila, en base64 sin comprimir |
| `Pieces` | `[]` | Capa de piezas: la lista de piezas |

### Piezas

Una pieza es una forma que se rellena con un material y se eleva `Height` píxeles. De esa altura salen el
sombreado que se hornea en el color y la normal que se guarda en `atlas_n.png`.

| Clave | Por defecto | Significado |
|---|---|---|
| `Shape` | `"box"` | `box`, `ellipsoid`, `capsule`, `polygon`, `line`, `pixel` o `paint` |
| `At` | `[0, 0]` | El centro (`box`, `ellipsoid`, `capsule`), el inicio (`line`, `paint`) o el píxel (`pixel`) |
| `To` | `[-1, -1]` | El final de `line` y `paint`. `[-1, -1]` significa lo mismo que `At` |
| `Size` | `[1, 1]` | Ancho y alto. En `line`, X es el grosor |
| `Points` | `[]` | Las esquinas de `polygon`, como `[[x, y], ...]` |
| `Material` | | Un nombre de la paleta (si no existe, se avisa y la pieza no se dibuja) |
| `Height` | 1 | Altura en píxeles |
| `Bevel` | 0 | Píxeles desde el borde en los que sube la altura (`box`, `polygon`). 0: borde recto |
| `Z` | 0 | Orden dentro de la celda: la de mayor Z tapa a las demás. Con la misma Z, se respeta el orden de la lista |
| `Emit` | -1 | Si es 0 o más, sustituye al `Emit` del material |

Las formas:
- `ellipsoid` y `capsule` son cúpulas: la altura baja hasta 0 en el borde.
- `line` y `pixel` son planas.
- `paint` cambia el color de lo que ya está dibujado en la celda dentro del rectángulo `At`..`To`, sin cambiar
  la altura. Sirve para ventanas, luces y detalles.

### Horneado

Las capas se componen de abajo arriba (mezcla alfa normal):
1. La normal de cada píxel sale de la pendiente de la altura de su capa. Fuera de la capa, la altura cuenta
   como 0, así que los bordes de una pieza se inclinan.
2. El color se sombrea con una luz fija que viene de arriba a la izquierda. Una superficie plana conserva su
   color exacto, y el factor va de 0,5 a 1,3.
3. La normal y el brillo se escriben donde la capa cubre al menos un 50 %.
4. Al final se dibuja el contorno (`Outline`) en los píxeles vacíos que tienen un vecino dibujado arriba,
   abajo, a la izquierda o a la derecha.

`atlas_n.png` guarda la normal en RGB (128 + 127·n, con la Y hacia abajo, como la pantalla) y el brillo en A.

> Versión provisional: el horneado de las piezas tiene que dar el mismo resultado que nyiengisprite, para que
> los `.sprite` de ProjectEGIDA se vean igual una vez importados. Se ajustará cuando se compare con su código.

### Etiquetas (animaciones) y sockets

Una etiqueta es una animación con los frames de `From` a `To`, los dos incluidos.

| Clave | Por defecto | Significado |
|---|---|---|
| `Name` | | Nombre de la animación |
| `From`, `To` | 0 | Frames de la animación (deben cumplir `From` ≤ `To`) |
| `Direction` | `"forward"` | `forward`, `reverse` o `pingpong` (0 1 2 1) |
| `Loop` | true | false: se para en el último frame |
| `Events` | `{}` | `{"nombre": [frames contados desde From]}` |

Cada socket tiene un `Name` y, en `Frames`, un punto por frame: `{"0": [x, y], "3": [x, y, grados]}`, en
píxeles del lienzo.

## Empaquetado

`PackSprites({{"heroe", sprite}, ...})` hornea todos los frames y los coloca por estanterías: los más altos
primero, con 1 píxel de separación, en un ancho que es potencia de dos. Los frames idénticos se guardan una
sola vez. El orden en que se pasan los sprites no importa, porque se ordenan por nombre.

Cómo se llaman en el atlas:
- Un sprite sin etiquetas y con un solo frame se llama `heroe`.
- Un sprite sin etiquetas y con varios frames da `heroe_0`, `heroe_1`... y la animación `heroe`.
- Cada etiqueta da los frames `heroe_<etiqueta>_0`, `heroe_<etiqueta>_1`... (con `pingpong` ya desplegado) y la
  animación `heroe_<etiqueta>`. Su `fps` es 1000 / el máximo común divisor de las duraciones, y `durations` va
  en esos ticks.
- Los sockets pasan a `sockets` de cada frame, medidos desde el pivote.

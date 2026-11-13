# Formato BitModel (`.bitmodel`), versión 1

Un modelo 3D low-poly para el 3D básico de BitMotor. Lo exporta BitModel y lo carga `Model::Load("robot.bitmodel")`
(`include/BitMotor/Render/Model.h`). El código que lo lee y lo escribe está en `src/Render/ModelFile.cpp`. Este
documento y ese fichero tienen que decir lo mismo.

Lo que el 3D de BitMotor dibuja en esta versión: mallas de triángulos con posición, normal, UV y color por vértice;
un material por malla (color, textura, emisión, sin luz); nodos en jerarquía; animación rígida por nodo; sockets.
La luz es ambiente + un sol + hasta 8 luces puntuales por modelo (las más cercanas), por píxel, con bandas
opcionales (`Lighting::SetStepped`). No hay esqueletos, morph targets, PBR ni sombras proyectadas.

JSON en UTF-8, claves en PascalCase. Una clave que falta toma su valor por defecto y una desconocida se avisa en el
log. Sistema de coordenadas: diestro, **Y hacia arriba**, el modelo mira hacia **-Z**, 1 unidad = 1 metro. Los
colores van en los mismos valores que `BitMotor::Color` (sRGB, 0..1). Las rotaciones son
cuaterniones normalizados `[X, Y, Z, W]`.

```json
{
  "Format": "bitmodel",
  "Version": 1,
  "Materials": {
    "wood":  {"Color": [0.55, 0.35, 0.2, 1]},
    "glow":  {"Color": [1, 0.9, 0.4, 1], "Emission": 1, "Unlit": true},
    "skin":  {"Texture": "hero.png", "Filter": "nearest"}
  },
  "Meshes": {
    "crate": {
      "Material": "wood",
      "Positions": [-0.5, -0.5, 0.5,  0.5, -0.5, 0.5,  0.5, 0.5, 0.5,  -0.5, 0.5, 0.5],
      "Normals":   [0, 0, 1,  0, 0, 1,  0, 0, 1,  0, 0, 1],
      "Uvs":       [0, 1,  1, 1,  1, 0,  0, 0],
      "Colors":    [1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1],
      "Indices":   [0, 1, 2,  0, 2, 3]
    }
  },
  "Nodes": [
    {"Name": "root", "Parent": -1},
    {"Name": "lid", "Parent": 0, "Mesh": "crate", "Position": [0, 0.5, 0], "Rotation": [0, 0, 0, 1], "Scale": [1, 0.2, 1]}
  ],
  "Animations": {
    "open": {
      "Duration": 0.5,
      "Loop": false,
      "Tracks": [
        {"Node": "lid", "Property": "Rotation", "Interpolation": "Linear",
         "Times": [0, 0.5], "Values": [0, 0, 0, 1,  -0.5, 0, 0, 0.866]}
      ],
      "Events": {"creak": [0.1]}
    }
  },
  "Sockets": {
    "handle": {"Node": "lid", "Position": [0, 0.1, -0.5], "Rotation": [0, 0, 0, 1]}
  }
}
```

| Nivel | Clave | Tipo (por defecto) | Significado |
|---|---|---|---|
| Modelo | `Format` | texto (obligatorio) | Tiene que ser `"bitmodel"`; si no, no se carga |
| | `Version` | entero (1) | Una mayor que 1 se carga con un aviso |
| | `Materials` | objeto ({}) | Materiales por nombre |
| | `Meshes` | objeto ({}) | Mallas por nombre |
| | `Nodes` | lista ([]) | Nodos; un padre siempre va antes que sus hijos |
| | `Animations` | objeto ({}) | Animaciones por nombre |
| | `Sockets` | objeto ({}) | Puntos de anclaje por nombre |
| Material | `Color` | [R, G, B, A] ([1, 1, 1, 1]) | Multiplica la textura y el color de vértice |
| | `Texture` | texto ("") | PNG relativo al `.bitmodel`; vacío, sin textura |
| | `Filter` | texto ("nearest") | `"nearest"` o `"linear"` |
| | `Emission` | número (0) | Se suma su color × 1.5 × `Emission` encima de la luz: brilla aunque esté a oscuras (y por encima de 1 entra en el bloom) |
| | `Unlit` | booleano (false) | Ignora la luz: se ve con su color tal cual |
| Malla | `Material` | texto ("") | Nombre del material; si no existe, blanco |
| | `Positions` | números | X, Y, Z por vértice (obligatorio) |
| | `Normals` | números ([]) | X, Y, Z por vértice; vacío, BitMotor calcula normales planas por cara |
| | `Uvs` | números ([]) | U, V por vértice, (0, 0) arriba a la izquierda de la textura; vacío, (0, 0) |
| | `Colors` | números ([]) | R, G, B, A por vértice; vacío, blanco |
| | `Indices` | enteros ([]) | Tres por triángulo, en sentido antihorario visto desde fuera; vacío, los vértices de tres en tres |
| Nodo | `Name` | texto ("") | Único dentro del modelo (lo usan animaciones y sockets) |
| | `Parent` | entero (-1) | Índice de su padre en `Nodes`; -1, raíz |
| | `Mesh` | texto ("") | Malla que dibuja; vacío, solo transforma a sus hijos |
| | `Position` | [X, Y, Z] ([0, 0, 0]) | Respecto a su padre |
| | `Rotation` | [X, Y, Z, W] ([0, 0, 0, 1]) | Cuaternión respecto a su padre |
| | `Scale` | [X, Y, Z] ([1, 1, 1]) | Respecto a su padre |
| Animación | `Duration` | segundos (el último tiempo de sus pistas) | Lo que dura una pasada |
| | `Loop` | booleano (true) | Repite o se queda en el final |
| | `Tracks` | lista ([]) | Pistas |
| | `Events` | objeto ({}) | `evento: [segundos]` |
| Pista | `Node` | texto | Nombre del nodo que mueve |
| | `Property` | texto | `"Position"`, `"Rotation"` o `"Scale"` |
| | `Interpolation` | texto ("Linear") | `"Linear"` (las rotaciones con slerp) o `"Step"` |
| | `Times` | números | Segundos, crecientes |
| | `Values` | números | 3 por tiempo (4 para `Rotation`) |
| Socket | `Node` | texto | Nodo al que va pegado |
| | `Position`, `Rotation` | como en los nodos | Respecto a ese nodo |

Reglas:
- Una animación solo cambia las propiedades que tienen pista; el resto se queda como en el nodo.
- Antes del primer tiempo vale el primer valor y después del último, el último.
- Escribir números con como mucho 6 decimales y las claves en el orden de este documento, para que los diffs de
  git sean pequeños y el fichero sea determinista.
- Las texturas son PNG RGBA de 8 bits en sRGB, como las de BitSprite. Una textura de paleta (una franja de colores
  y UVs que apuntan a cada color) es la forma recomendada de colorear modelos low-poly.

## Cómo se dibuja

- Cada nodo con malla se dibuja con `Mat4::Transform(Position, Rotation, Scale)` multiplicado por el de su padre, y
  todo por la posición, rotación y escala del `Model3D`.
- Cara delantera en sentido antihorario visto desde fuera; las traseras no se dibujan.
- Color final: `textura × color de vértice × Color del material × Tint del Model3D`. Si `Unlit`, ese color se ve
  tal cual (más la emisión); si no, se multiplica por la luz `ambiente + sol × max(0, n · −dirección)` + cada luz
  puntual `color × (1 − d / radio)² × (0.25 + 0.75 × max(0, n · l))`, y se le suma `color × 1.5 × Emission`.
- Con `Config::LinearColor`, los colores y las texturas se convierten de sRGB a lineal antes de todo esto.
- Los modelos con transparencia (alfa del color del material o del `Tint` menor que 1) se dibujan después de los
  opacos, del más lejano al más cercano.

## Hacer y cambiar modelos por código

BitMotor tiene todo lo necesario para crear modelos sin ventana; BitModel (la aplicación) solo pone la interfaz
encima, y un juego o una herramienta pueden usar lo mismo:

- `ModelFile` (`include/BitMotor/Render/ModelFile.h`) es el `.bitmodel` tal cual, con los nombres y las rutas de
  textura: `ModelFile::Load`, `Parse`, `ToJson`, `Save`. `Model::FromFile(file)` lo convierte en un `Model` para
  dibujar (`Model::Parse` es lo mismo en un paso). Una referencia que no existe se conserva en el fichero; al
  dibujar se avisa y se ignora.
- `Save` escribe de forma determinista: las claves en el orden de este documento, los nombres ordenados, sin los
  valores por defecto, números con como mucho 6 decimales y un vértice (o un triángulo) por línea. Leer y volver
  a guardar da los mismos bytes.
- `Modeling::` (`include/BitMotor/Render/Modeling.h`): primitivas, ediciones de malla por caras (extruir,
  insetar, subdividir, espejo, pintar, UV de paleta...), cambios del modelo que mantienen las referencias
  (renombrar, quitar nodos, cambiar de padre), claves de animación, `Pose` sin crear el modelo, `Check` (lo que
  estaría mal) e importar y exportar OBJ con su MTL y glTF 2.0 (`ImportGltf` lee `.glb` y `.gltf`; `ExportGlb`
  escribe `.glb`, con los sockets y los eventos en sus `extras` para que vuelvan al importarlo). Lo que BitMotor no
  dibuja (huesos, morph targets, mapas PBR, curvas cúbicas) se deja fuera o se aproxima y queda en `Notes`.

```cpp
ModelFile file;
file.Meshes["tower"] = Modeling::Cylinder(0.5f, 2, 8);
ModelFile::Mesh& tower = file.Meshes["tower"];
auto top = Modeling::Inset(tower, Modeling::FacesFacing(tower, {0, 1, 0}), 0.3f);
Modeling::Extrude(tower, top, -0.2f);                  // un hueco arriba
Modeling::Paint(tower, top, Colors::Slate);
ModelFile::Node node;
node.Name = node.Mesh = "tower";
file.Nodes.push_back(node);
Modeling::SetKey(file, "idle", "tower", ModelFile::Property::Scale, 0.5f, std::vector<float>{1, 1.1f, 1});
file.Save("tower.bitmodel");
```

Las caras son triángulos, numerados por su sitio en `Indices`. Las ediciones que necesitan que una cara tenga sus
propios vértices (pintar, UV, voltear) los copian antes, así que las caras vecinas no cambian.

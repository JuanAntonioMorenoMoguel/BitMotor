# UI::JFxStyle — Estética inspirada en JavaFX (Modena)

Submódulo de la UI de BitMotor que **solo imita el aspecto** de una aplicación de escritorio JavaFX con su tema
Modena: controles gris claro con un degradado vertical suave, esquinas redondeadas, bordes finos con un brillo
blanco interior, sombras suaves bajo menús y diálogos, el anillo azul de foco con su halo y las selecciones en
azul. Es para editores y herramientas hechos con BitMotor; el primero será **BitMusic** (el editor tipo tracker).

`#include <BitMotor/BitMotor.h>` ya lo trae: `BitMotor::UI::JFxStyle` (en los ejemplos, `namespace Fx = UI::JFxStyle;`).
Ejemplo completo: `examples/14_JFxStyle`.

## Qué es y qué no es

| Sí | No |
|---|---|
| Interfaz **inmediata**, como el resto de `UI::*`: se llama a los widgets cada fotograma en `Draw` y cada uno se dibuja y responde ahí | Grafo de escena, nodos, árbol de controles, CSS, *bindings* o propiedades observables |
| Los colores son un `struct Palette` con los nombres de los colores de Modena (`-fx-base`, `-fx-accent`...) | Hojas de estilo ni selectores |
| Comparte foco, clics, ventanas modales, `Tooltip` y `OnClick` con `UI::*`: los dos tipos de widget se mezclan en la misma pantalla | Gestor de ventanas o de diseño (*layout*): las áreas las da el juego, como en `UI::*` |

## Uso

```cpp
namespace Fx = UI::JFxStyle;

void Start() override {
    Fx::Palette palette = Fx::Modena();                         // o Fx::Modena(base, acento)
    palette.Text = {.Font = Font::Load("fonts/Inter.otf", {.Size = 14})};
    Fx::SetPalette(palette);
    UI::SetTheme(Fx::Theme());                                  // UI::* y tooltips con los mismos colores
}

void Draw() override {
    Fx::Background();
    Fx::BeginMenuBar({0, 0, Screen::Width(), 28});
    if (Fx::Menu(Tr("menu.file"))) {
        if (Fx::MenuItem(Tr("menu.save"), "Ctrl+S")) Save();
        Fx::MenuSeparator();
        if (Fx::MenuItem(Tr("menu.quit"))) Quit();
        Fx::EndMenu();
    }
    Fx::EndMenuBar();

    Rect left, right;
    Fx::SplitPane({0, 28, Screen::Width(), Screen::Height() - 28}, split, left, right);
    Fx::ListView(left.Expand(-8), instrument, names, scroll);
    Rect page = Fx::TabPane(right.Expand(-8), tab, {Tr("tab.pattern"), Tr("tab.song")});
    if (Fx::Button({page.X + 8, page.Y + 8, 100, 28}, Tr("play"))) Play();
    UI::Tooltip(Tr("play.tip"));
}
```

Pensado para una resolución lógica de escritorio (p. ej. 1280×720) y una fuente TrueType/OpenType en
`Palette::Text` (el ejemplo usa Inter, licencia OFL). Con `Font::Pixel` también funciona; conviene bajar
`Palette::Padding`.

## Paleta

`Palette` tiene por defecto los colores de Modena. `Modena(base, accent)` los deriva de otro `-fx-base` y
`-fx-accent` con las mismas fórmulas que Modena (`derive()` y `ladder()` de JavaFX), así que una base oscura da un
tema oscuro con texto claro:

```cpp
Fx::SetPalette(Fx::Modena(Color::Hex(0x3C3F41), Color::Hex(0x4A88C7)));
```

| Campo | Modena | Para qué |
|---|---|---|
| `Text` | — | Fuente y tamaño de todas las etiquetas (su `Color` no se usa) |
| `Base` | `#ECECEC` | `-fx-base`: los controles son tonos de él |
| `Background` | `#F4F4F4` | `-fx-background`: ventana, paneles, contenido de pestañas |
| `InnerBackground` | `#FFFFFF` | `-fx-control-inner-background`: campos de texto, listas |
| `Hover`, `Pressed` | `#F8F8F8`, `#DEDEDE` | Base bajo el ratón y pulsada |
| `OuterBorder`, `TextBoxBorder`, `BoxBorder` | `#B6B6B6`, `#CFCFCF`, `#C9C9C9` | Bordes de controles, de campos de texto y de cajas/divisores |
| `TextColor`, `SelectedText`, `PromptText` | `#333333`, blanco, `#A6A6A6` | Texto normal, sobre el acento y el gris de un campo vacío |
| `Mark` | `#575757` | Marca de `CheckBox`, punto de `RadioButton`, flechas |
| `Accent` | `#0096C9` | Selecciones, opción de menú bajo el ratón, barras de progreso |
| `SelectionUnfocused` | `#D3D3D3` | Selección de una lista sin el foco |
| `Focus`, `FaintFocus` | `#039ED3`, `#039ED322` | Anillo de foco y su halo |
| `DefaultButton` | `#ABD8ED` | Base de `DefaultButton` |
| `Shadow`, `ShadowSize` | negro 20 %, 8 | Sombra de menús, listas desplegables y diálogos |
| `TooltipBack`, `TooltipText` | `#1E1E1ECC`, blanco | Tooltips (vía `Theme()`) |
| `ModalShade` | negro 30 % | Detrás de un diálogo |
| `Radius`, `Padding`, `DisabledOpacity` | 3, 8, 0.4 | Esquinas, margen del texto y opacidad de lo desactivado |

`Theme()` devuelve un `UI::Theme` con estos colores para los widgets de `UI::*`, sus tooltips (ahora con
`Theme::TooltipText`) y sus ventanas modales. `Derive(color, cantidad)` es el `derive()` de JavaFX.

## Formas

Todo se dibuja con el píxel blanco, como `UI::Rect`, en la capa `Hud`, con los bordes en píxeles enteros y las
esquinas suavizadas (cobertura 4×4 por píxel).

- `RoundedRect(area, radio, color)` y `RoundedRect(area, radio, arriba, abajo)` (degradado vertical).
- `RoundedOutline(area, radio, color, grosor)`.
- `Shadow(area, radio, color, tamaño)`: sombra suave, un poco desplazada hacia abajo. Se dibuja antes de lo que la proyecta.
- `ControlBody(area, base, radio)`: el cuerpo de un control de Modena (borde exterior, brillo interior y degradado) para widgets propios.
- `FocusRing(area, radio)`: el anillo de foco con su halo.

## Widgets

| Widget | Devuelve | Notas |
|---|---|---|
| `Background(area = pantalla)` | — | Color de fondo de la ventana |
| `Label(area, texto, align)` | — | Centrado en vertical |
| `Separator(area)` | — | Horizontal o vertical según la forma del área |
| `Button`, `DefaultButton` | `true` al pulsarse | `DefaultButton`: el botón de Enter de un diálogo |
| `ToggleButton(area, texto, bool&)` | `true` al cambiar | Para silenciar/solo, herramientas |
| `CheckBox(area, texto, bool&)` | `true` al cambiar | Izquierda/Derecha también lo cambian |
| `RadioButton(area, texto, int& elegido, int valor)` | `true` al elegirse | Un grupo comparte `elegido` |
| `Slider(area, float&, min, max, paso)` | `true` mientras cambia | Pista hundida y botón redondo; arrastre o Izquierda/Derecha |
| `ProgressBar(area, fracción)` | — | Relleno con el acento |
| `TextField(area, etiqueta, std::string&, TextFieldOptions)` | `true` al cambiar | Edita igual que `UI::TextField` (usa `TextFieldOptions::Frame = false` por dentro) |
| `ComboBox(area, int&, opciones)` | `true` al cambiar | Lista desplegable con sombra; Izquierda/Derecha sin abrirla |
| `ListView(area, int&, elementos, int& scroll)` | `true` al cambiar | Filas alternas, barra de desplazamiento, rueda del ratón. Solo dibuja filas enteras |
| `Panel(area, título)` | área interior | Caja con barra de título (un `TitledPane` siempre abierto) |
| `TabPane(area, int&, etiquetas)` | área del contenido | Pestañas arriba; se dibuja el contenido de la elegida |
| `SplitPane(area, float& proporción, Rect& a, Rect& b, Orientation)` | `true` mientras se arrastra | `Horizontal`: lado a lado (como en JavaFX); `Vertical`: uno encima del otro |
| `BeginDialog(area, título)` / `EndDialog()` | área interior | Sombra, oscurece el resto y lo vuelve inactivo (`UI::BeginModal`) |

### Menús

Un solo menú abierto a la vez, dibujado encima de todo con sombra. Mientras está abierto se queda con todos los
clics y la navegación (lo de debajo no recibe nada); se cierra al elegir, con Escape, con un clic fuera o cuando un
fotograma deja de llamarlo.

```cpp
Fx::BeginMenuBar(area);
if (Fx::Menu(Tr("menu.view"))) {
    if (Fx::CheckMenuItem(Tr("menu.dark"), dark, "Ctrl+D")) UsePalette();
    Fx::MenuSeparator();
    if (Fx::MenuItem(Tr("menu.about"))) about = true;
    Fx::EndMenu();
}
Fx::EndMenuBar();

if (Fx::BeginContextMenu(patternArea)) {        // se abre con clic derecho donde se pulsó
    if (Fx::MenuItem(Tr("edit.copy"), "Ctrl+C")) Copy();
    Fx::EndMenu();
}
```

- `Menu(texto)`: título en la barra; abre con clic o Confirm. Con un menú abierto, señalar otro título lo abre y
  Izquierda/Derecha pasan al de al lado.
- `MenuItem(texto, atajo, activo)`: el atajo solo se muestra; las teclas las lee el juego.
- `CheckMenuItem(texto, bool&, atajo, activo)`, `MenuSeparator()`, `EndMenu()`.

## Teclado y foco

Igual que en `UI::*`: el ratón da el foco al pasar por encima, y las flechas (o el mando) lo mueven por los
widgets en el orden en que se llaman; Confirm los pulsa. El anillo azul de foco se ve cuando el foco lo ha movido
el teclado o el mando. Dentro de un menú o de la lista de un `ComboBox`: Arriba/Abajo, Confirm y Escape.

## Cambios en la UI base

- `UI::Theme::TooltipText`: color del texto de los tooltips (transparente: `Text.Color`, como antes).
- `UI::TextFieldOptions::Frame`: `false` dibuja solo el texto y el cursor, para quien dibuja la caja.
- `UiInternal.h` expone a otros ficheros de BitMotor las reglas de los widgets (`Detail::Interact`), un *popup*
  que captura la entrada (`Detail::OpenPopup`, `Popup`...) y `Detail::DrawLast` para dibujar encima de todo.

## Pendiente (posibles mejoras)

- Submenús, `TreeView`, `TableView`, `Spinner` y `ColorPicker`.
- Recorte (*scissor*) en la capa `Hud`, para listas con filas a medias y paneles con scroll.
- Widgets propios de tracker para BitMusic (rejilla de patrón, lista de orden) sobre estas formas.

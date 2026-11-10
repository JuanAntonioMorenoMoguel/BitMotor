# BitMotor — Contexto del proyecto

Librería C++ para crear juegos de forma sencilla. Unifica **nyiengilib** (motor actual) en un solo proyecto y será la base a la que se migren **ProjectEGIDA**, **MONOCHROME** y **RogueInvasion**. El futuro de **nyiengisprite** está pendiente de decidir.

## Decisiones

| Tema | Decisión |
|---|---|
| Idioma del código | Inglés |
| Estilo | PascalCase en tipos, funciones, métodos, campos y valores de enum; camelCase en parámetros |
| Namespace / include | `namespace BitMotor`, `#include <BitMotor/BitMotor.h>` |
| Filosofía | Cuanto más simple, mejor. Los módulos del motor se usan como funciones (`Input::Pressed`), sin managers visibles |
| Arquitectura del juego | **Servicios y managers** (no árbol de nodos): `Game` → `Services` (globales) + pila de `Scenes` → `Managers` de escena, `Events` para desacoplar y `Tasks` para secuencias |
| Pasar estado entre escenas | Servicio (todo el juego), datos por constructor (por defecto), `std::move` de un `unique_ptr` (un tramo, p. ej. una run) o `Push` + referencia (escena encima). No existe un `ManagerService` |
| Tests | Internos de BitMotor (`BITMOTOR_TESTS`, OFF como subdirectorio). No se ofrece nada para testear los juegos |
| Guardados | Los juegos adoptan el formato de BitMotor (`BITMOTOR_FIELDS` + `Data::Save`). Sin sistema de migración en la librería |
| Temblor de cámara | Dentro de `Camera2D` (`AddShake`, `Trauma`, `ShakeOffset`, `ShakeAngle`). Sin struct aparte |
| Settings | Estándar y ampliable por herencia (`struct MySettings : Settings`) |
| Texto en renderers propios | `Text::Layout` devuelve posiciones de glifos |
| 3D | 3D básico (`Model.h`): mallas, modelos `.bitmodel` de BitModel, cámara, sol y luces puntuales, debajo del 2D. Sin esqueletos ni sombras de momento |
| Platform/Store | Sí, todo bajo `Platform::`: backend local ya hecho; Steam aparcado hasta que haya un juego publicado. Workshop, consolas y multijugador masivo, fuera |
| Renderer propio | 3 niveles: Renderer2D, Renderer2D + pases de post-proceso, `CustomRenderer` sobre `Gpu` |

## Módulos y API

### Core
- **Arranque:** `Config { Title, Width, Height, WindowWidth, WindowHeight, IntegerScaling, Fullscreen, VSync, TickRate, Icon, Organization, Version, SteamAppId, Renderer }`, `Run<T>(Config)`, `Run<Game, Renderer>`, `Quit()`, `Game { Start, Stop, DebugPanel }`.
- **Services:** `Service { Start, Update, Stop }`, `Services::Add<T>, Get<T>, Find<T>, Has<T>, Remove<T>`.
- **Scenes:** `Scene { Enter, Exit, Resume, Update, Draw, UpdateBelow, DrawBelow, Add<T>, Get<T>, Run(Sequence) }`, `MakeScene<T>(args)`, `Scenes::Push, Pop, Replace, Clear, Current`, `Transition::None / Fade / Wipe(seconds, side, color) / Slide(seconds, side)` (`Transition::Left, Right, Up, Down`).
- **Managers:** `Manager { Start, Update, Draw, Stop, Owner, Active }` (se actualizan en orden de alta).
- **Events:** `Events::On<T>` → `Subscription`, `Emit<T>`, `Queue<T>`. Del motor: `WindowResized, GamepadConnected, GamepadDisconnected, FileChanged, SceneChanged, FocusChanged, OverlayChanged`.
- **Tasks:** `Sequence().Wait, WaitUntil, Do, Tween, Parallel, Repeat`, `Tasks::Run` → `Task { Cancel, Done }`.
- **Time:** `Delta, RealDelta, Total, Frame, SetScale, HitStop, Alpha`; `Timer { Start, Update, Done, Progress }`.
- **Window / Screen:** `Window::SetTitle, Size, SetFullscreen, ToggleFullscreen, IsFullscreen, SetVSync`; `Screen::Width, Height, Center, Clear, ToLogical, Screenshot, Split(n)`.
- **Input:** `Bind, Down, Pressed, Released, Axis, RightStick, KeyDown, KeyPressed, Mouse, MouseDelta, MouseWheel, MouseDown, MousePressed, UsingGamepad, Rumble, Rebind, IsRebinding, KeyName, ButtonName, LoadBindings, SaveBindings, Force, StartRecording, StopRecording, Replay`. Local: `SetPlayers, Player(i)`, `AssignNextDevice`.
- **Data / Files:** `BITMOTOR_FIELDS(Type, …)`, `Data::Load<T>, Save<T>, Open` → `Document { Get, Has, Ok }`; `Files::Read, Write (atómica), Exists, Watch, AssetPath, SavePath`.
- **Settings, Log, Args:** `Settings` (volúmenes, pantalla, VSync, temblor, destellos, tamaño de texto, vibración, idioma) + `Save, Load, Reset`; `Log::Info, Warn, Error`; `Args::Has, Get`.

### Math
`Vec2, Vec3, Vec4, Mat4, Quat`, `Pi, Tau`, `Min, Max, Clamp, Abs, Sign, Lerp, Remap, Approach, Damp, Wrap`, `Length, Distance, Normalize, Dot, Cross, Rotate, AngleOf, FromAngle, WrapAngle`, `Rect`, `Circle`, `Ease::*`, `Tween`, `Random` (PCG32), `Noise1D`.

### Colors
`Color { R, G, B, A, Hex, WithAlpha, Lighter, Darker, Lerp }` y 40 `Colors::`: White, Black, Gray, LightGray, DarkGray, Red, DarkRed, Crimson, Maroon, Coral, Salmon, Orange, Amber, Gold, Yellow, Lime, Green, DarkGreen, Olive, Mint, Teal, Turquoise, Cyan, SkyBlue, Blue, Navy, Indigo, Purple, Violet, Lavender, Magenta, Pink, Rose, Brown, Tan, Beige, Slate, Phosphor, Ice, Transparent.

### Audio
Formatos: WAV, FLAC, MP3, OGG y `.bitmusic` (canciones de BitMusic, sintetizadas por el motor).
`Sound { Load, Synth, Play(volume, pitch, pan), SetMaxVoices }`, `Voice { Advance, … }`, `Music { Play, Procedural, Stop, IsPlaying, SetParam, Beat, Pulse }`, `Audio::SetVolume(Bus), SetPaused`, `Synth::Sine, Square, Saw, Triangle, Noise, LowPass, HighPass, Bitcrush, Fall, AttackRelease, Decay, NoteToHz`.

### Textures
`Filter { Nearest, Linear }`, `Texture { Load, FromPixels, Width, Height, Size, Full }`, `RenderTexture { Create, Begin(clear), Continue, End, Source, Full }` (dibujar sprites, texto y formas en una textura), `Atlas { Load, Sprite, Has, Animation, Socket }` (formato nyiengisprite), `Animation { Frame, Done, Duration, Fired }`.

### Obj 2D
`Layer { Background, World, Glow, Overlay, Hud }`, `Sprite2D { Region, Position, Rotation, Scale, Origin, Color, Flip, Flash, Layer, Depth, Material }`, `Draw(Sprite2D)`, `Camera2D { Position, Zoom, Rotation, Viewport, AddShake, Trauma, ShakeOffset, ShakeAngle, Update, ToWorld, ToScreen }`, `SetCamera`, `Light2D { Position, Height, Radius, Color, Intensity }`, `Screen::Draw(Light2D)`, `Lighting::Enable, SetAmbient, SetStepped, SetBloom`, `ScreenEffects { Vignette, Grain, Aberration, Saturation }`, `Screen::SetEffects, Flash`, `Texture::SetNormals` (atlas_n.png), `ParticleRange { Min, Max }`, `ParticleStyle { Region, Life, Speed, Direction, Spread, Gravity, Drag, StartSize, EndSize, StartColor, EndColor, Spin, RandomAngle, Layer, Depth }`, `Particles { Emit, Update, Draw, Clear, Count, Capacity }`.

### UI (inmediata)
`Rect, RectOutline, Circle, Line, Triangle, Polygon`, `Panel, Button, Toggle, Slider, Dropdown, TextField (TextFieldOptions { Placeholder, MaxLength, Password, Enabled }), Typing, Bar, Segments, Image, Tooltip, Prompt, BeginModal, EndModal, Hovered, Clicked, SetTheme, OnClick`.

### Text
`Font::Pixel`, `Font::Load / FromTtf` (TrueType y OpenType con `TtfOptions { Size, Smooth, Extra }`), `Font::FromAtlas`, `TextStyle { Font, Size, Color, Align, Shadow, Outline, LineHeight }`, `Draw, Paragraph, Rich, Typewriter, Measure, Wrap, Layout`, `Number(v, suffix), Percent, Clock`, `SetLanguage, Language, Tr` (`lang/<code>.json`).

### Platform
Hecho (local, `platform.json` en `Files::SavePath`): `Platform::Name, Save`, `Unlock, Unlocked, Progress, ProgressOf, UnlockedOn, Achievements, ResetAchievements` + evento `AchievementUnlocked`, `Stat, SetStat, AddStat, MaxStat`, `ConfigureBoard, Submit, Top, Best, ClearBoard` (`LeaderboardEntry`, `LeaderboardOptions`), `OwnsDlc, Dlcs` (carpetas `dlc/<id>`). Con Steam (aparcado): `PlayerName, PlayerId`, `IsSteamDeck, ShowKeyboard`. Con internet (fase 16): marcadores online, `CloudSave, CloudLoad`, `SetPresence`, `CreateLobby, FindLobbies, JoinLobby, SetLobbyData, InviteFriends`. El SDK de Steamworks no va en el repositorio (`-DBITMOTOR_STEAM_SDK`).

### Gpu y shaders
- `CustomRenderer { Init, Resize, Render, Shutdown }` (es un servicio). El motor sigue gestionando swapchain, ImGui, capturas y redimensionado.
- `Gpu::Native()`, `Shader::Load`, `Pipeline::Create`, `Buffer::Create`, `Buffer::Upload / Readback`, `Target::Create` (`PingPong`), `Gpu::SetGlobal`.
- Compute: `ShaderStage::Compute`, `ComputePipeline::Create`, `Frame::BeginCompute(buffers, targets)`, `Bind`, `Dispatch`, `End`. Todos los buffers y los targets (salvo sRGB) se pueden escribir desde compute.
- Recarga en caliente (Debug): los shaders cargados de un `.spv` de `BitMotor_Shaders` se recompilan al guardar su fuente (o un `.glsl` de su carpeta) y sus pipelines se rehacen; si hay errores se registran y se mantiene el código anterior.
- `Config::LinearColor`: mezcla y luz en espacio lineal (pantalla sRGB). Colores y texturas siguen en sRGB al escribirlos.
- `Run<Juego, Renderer>(config)`. `Frame`: `Begin/End` de pases (hasta 4 targets), `Bind`, `BindTexture`, `BindStorage`, `BindVertices/Indices`, `Push` (bloques 1..3; el 0 es `Gpu::Globals`), `Draw`, `DrawIndexed`, `Fullscreen` y `Draw2D` (los sprites, texto y UI de BitMotor encima).
- `CustomRenderer.h` y `Gpu.h` no están en `BitMotor.h`: nombres como `Frame`, `Target` o `Buffer` no chocan con los del juego.
- Panel de depuración: Dear ImGui, F1 o `--debug`; `Game::DebugPanel()` y `Scene::DebugPanel()`.
- `Frame { Begin, End, Bind, Push, Draw, DrawIndexed, Fullscreen, Dispatch, Swapchain, Draw2D }`.
- `Material2D { Load, With }`, `Renderer2D::AddPostPass, RemovePostPass, SetPostParam`, `ShaderRenderer(fragment)`.
- `#include <BitMotor.glsl>` con uniforms estándar, recarga en caliente, errores en magenta, panel automático de parámetros y `BitMotor_Shaders()` en CMake.

### Obj 3D
`Vertex3D`, `Material { Tint, Image, Emission, Unlit }`, `Mesh { Create, Cube, Sphere, Plane, Cylinder }`, `Model { Load, Parse, FromMesh, Animation, Animations, Nodes, Pose, Socket }` (formato en `docs/BitModel.md`), `ModelAnimation { Duration, Loops, Fired }`, `Model3D { Source, Position, Rotation, Scale, Tint, Animation, Time }`, `Camera3D { View, Projection, ViewProjection, ToScreen, Ray }`, `PointLight3D`, `Screen::SetCamera3D, Draw(Model3D), Draw(PointLight3D)`, `Lighting3D::SetSun, SetAmbient`. Matemáticas: `Quat { FromAxisAngle, FromEuler, LookRotation, Slerp }`, `Mat4::RotateX/Y, Rotate, Transform, Perspective, LookAt, TransformDirection`.

### Net
`Host, Join, Leave, IsHost, LocalId, Peers, Send<T>, On<T>, Ping`, eventos `PeerJoined, PeerLeft, ConnectionLost`, serialización binaria con `BITMOTOR_FIELDS`, `Lockstep`. Después: `Snapshot<T>, Interpolated<T>, Rollback<State>`, servidor dedicado.

## Roadmap

| Fase | Contenido | Hito |
|---|---|---|
| 0 | Proyecto base: repo, CMake, vcpkg, carpetas por módulo, `BitMotor_Prepare`, opciones de tests y ejemplos | |
| 1 | Core fundamental: arranque, Window, Screen, Time, Input, Events, Files, Data, Log, Args, Math, Colors | |
| 2 | Arquitectura: Services, Scenes, Managers, Tasks, Settings | |
| 3 | Render 2D: Renderer2D, Textures, Sprite2D, Camera2D, Light2D, Particles | |
| 4 | Text y UI | |
| 5 | Audio y formato `.bitmusic` (lector, escritor y sintetizador) | |
| 6 | Platform y Store sin internet (local + Steam: logros, estadísticas, marcadores locales, DLC) | |
| 7 | `CustomRenderer` + `Gpu::Native` + `BitMotor_Shaders` | **v1.0** |
| 8 | Migrar ProjectEGIDA | |
| 9 | Migrar MONOCHROME (TickRate 120, HitStop, `MonoRenderer`, escenas, `WorldManager` con EnTT) | |
| 10 | Shaders: Material2D, post-proceso, uniforms estándar, recarga, Gpu simplificado, `Draw2D` | **v1.1** |
| 11 | Multijugador local | |
| 12 | Compute + ShaderRenderer | **v1.2** |
| 13 | Migrar RogueInvasion (depende de nyiengisprite) | |
| 14 | 3D y extras aplazados: Models, Obj 3D, TTF, RenderTexture, Dropdown, TextField, Wipe/Slide | Hecha |
| 15 | BitMusic: la aplicación (hecha con BitMotor), BitMotor en web (Emscripten, renderer para el navegador) y widgets de tracker. El formato `.bitmusic` ya existe desde la fase 5 | |
| 16 | Internet: Net online (Steam, lobbies, lockstep), red avanzada, y de Platform los marcadores online, la nube (`CloudSave`, `CloudLoad`) y la presencia | **v2.0** |

Cada fase termina con tests internos y un ejemplo en `examples/`.

## Pendiente
- Integración con Steam (aparcada hasta que haya un juego publicado; falta el App ID).
- Multijugador local (fase 11, aparcada).
- BitSprite, BitMusic (la aplicación) y BitModel se hacen en sus propios proyectos y repositorios.

## Código existente (referencia)
- `nyiengilib` y `nyiengisprite` en `C:\Users\juamo\Desktop\NYIENGI`.
- Juegos en GitHub: `JuanAntonioMorenoMoguel/ProjectEGIDA`, `/MONOCHROME`, `/RogueInvasion`.

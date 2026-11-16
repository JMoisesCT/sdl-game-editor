[English](README.md) | **Español**

# SDL Game Editor — editor y motor 2D educativo sobre SDL3

Motor de videojuegos 2D con **editor integrado**, construido sobre SDL3 en C++17. Su
propósito es **educativo**: que estudiantes que ya conocen SDL3 básico armen juegos
editando escenas en un editor propio, sin pelear con la API cruda de SDL.

> **Estado:** en desarrollo, por fases. Ya están el núcleo (ECS + reflexión + escenas en
> JSON), el editor, los tilemaps (se pintan con paleta en el propio editor), la física con
> el motor de plataformas, los prefabs, las reglas de juego (vida, daño, frutas,
> checkpoints), la presentación (animación por reglas, cámara que sigue, fondo con
> parallax, texto y HUD), el audio y los mecanismos del escenario (plataformas de un
> sentido, móviles y que caen, y trampolines). La escena de ejemplo es un nivel de
> plataformas completo. Siguen los ejemplos cenital y de naves.

> La interfaz del editor está en inglés: en este documento, los botones y paneles se
> nombran como aparecen en pantalla (**Play**, **Hierarchy**, **Add component**...).

## Cómo está pensado

- **La escena es un documento.** Todo lo que hay en ella se guarda en un `.scene.json`, y
  el editor trabaja directamente sobre esos datos: lo que ves es lo que se guarda.
- **ECS propio** (entidades, componentes y sistemas), escrito para leerse en clase:
  - **Entidad**: solo un número.
  - **Componente**: un `struct` de datos sin lógica (`Transform`, `Sprite`, `Camera`...).
  - **Sistema**: el comportamiento; recorre las entidades con ciertos componentes y las
    modifica. Solo corre mientras se juega.
- **Reflexión mínima.** Cada componente declara sus campos una vez:

  ```cpp
  registry.add<Transform>("Transform")
      .field("position", &Transform::position)
      .field("rotation", &Transform::rotation).range(-360, 360)
      .field("scale",    &Transform::scale);
  ```

  Con esa lista el motor lo guarda y carga en JSON, y el Inspector del editor lo muestra y
  edita. Un componente nuevo aparece en el editor **sin tocar el editor**. Un campo puede
  ser también una **lista de structs** (los clips de una animación), declarada con
  `.list(...)`; un texto puede ofrecer **opciones** (`.choices(...)`) que el Inspector
  muestra como lista desplegable, y un campo de tipo `Entity` es una **referencia a otra
  entidad** de la escena (se elige de una lista o arrastrándola desde la Hierarchy).
- **La aplicación es dueña del bucle.** El juego no escribe su `main` de SDL: registra sus
  componentes y sistemas, y abre una escena.

  ```cpp
  int main(int argc, char* argv[]) {
      Application app;
      if (!app.init({ "My Game", 1280, 720 })) return 1;
      app.registry().add<MyComponent>("MyComponent").field(/* ... */);
      app.addSystem(std::make_unique<MySystem>());
      app.openScene("assets/scenes/level1.scene.json");
      return app.run();
  }
  ```

### Tu propio código

Para sumar comportamiento propio con valores editables en el Inspector (un componente, su
registro y un sistema, todo en `game/`), ver la guía paso a paso
**[docs/GUIA_CODIGO_PROPIO.md](docs/GUIA_CODIGO_PROPIO.md)**: un ejemplo completo, los
tipos de campo, referencias a otras entidades, condiciones para animación y sonido, y los
errores más comunes.

## El editor

- **Hierarchy** (izquierda): las entidades de la escena en **árbol**; crear, duplicar y
  borrar. Las instancias de prefab se ven en azul. Ver [Jerarquía](#jerarquía).
- **Prefabs** (izquierda, abajo): los prefabs de `assets/`; doble clic o arrastrar a la
  vista para colocar uno.
- **Inspector** (derecha): los componentes de la entidad elegida, editables; agregar y
  quitar componentes.
- **Vista** (centro), con dos pestañas:
  - **Scene**: cámara libre del editor, grilla, selección y arrastre.
  - **Game**: lo que ve el jugador. Arriba a la derecha, una lista desplegable elige cómo
    se escala: **Pixel perfect** (por defecto), **Fit** o **100%**.
- **Play / Pause / Stop**: al detener, la escena vuelve exactamente a como estaba antes de
  jugar.

### Jerarquía

Las entidades se pueden **anidar** para ordenar la escena en grupos ("Fruits", "Traps",
"HUD"...). Es una jerarquía de **organización**: el `Transform` de cada entidad sigue siendo
su posición en el mundo, así que la física, el dibujo y las reglas no cambian.

- **Arrastra** una entidad sobre otra para hacerla su **hija**; sobre el borde de arriba o de
  abajo de otra, para ponerla **antes** o **después** (como hermana). Soltarla en el hueco
  libre de abajo la saca de su grupo. El menú del clic derecho ofrece **Create child**
  (crear hija), **Group** (agrupar) y **Remove from group** (sacar del grupo).
- **Elegir varias** con `Ctrl+clic` (suma o quita una) o `Mayús+clic` (un rango). Arrastrar
  una de ellas se lleva a todas, en su orden; **Duplicate**, **Delete** y **Remove from
  group** actúan sobre todas. El Inspector muestra la última elegida.
- **Group** (`Ctrl+G`) crea una entidad vacía en el lugar de la primera elegida y mete
  adentro toda la selección: una carpeta de un solo paso. Queda elegida, lista para
  ponerle nombre.
- Un grupo también puede ser una entidad vacía hecha a mano (**+ New**, sin sprite).
- **Mover al padre** (en la vista o en el Inspector) mueve también a toda su rama, y
  **Duplicate** o **Delete** se llevan la rama entera.
- **Jugando**, los hijos **no** siguen al padre (si un sistema destruye al padre, los hijos
  quedan sueltos; para llevárselos, `destroyWithChildren`).
- Un prefab es **una** entidad: al crear un prefab desde un padre, sus hijas no entran.

En el archivo, una hija lleva el id de su padre: `{ "id": 7, "parent": 3, "components": {...} }`,
y las entidades se guardan en el orden del árbol.

### Tilemaps

Un nivel de tiles se arma con entidades que tienen el componente **`Tilemap`**: cada una es
una **capa** (fondo, suelo, decoración...). Como cada capa tiene su propio `order`, se
intercala con los sprites: el jugador puede quedar delante del suelo y detrás de unas
plantas. Su `Transform` marca la **esquina superior izquierda** del mapa (no el centro, como
en los sprites).

- Elige una capa: el Inspector muestra la **paleta** del tileset. Clic en un tile para
  elegirlo, o arrastra para elegir un bloque de varios.
- Herramientas: **Brush** (pincel, `B`), **Eraser** (goma, `E`), **Fill** (relleno, `G`) y
  **Move** (mover, `V`). `Alt+clic` en la vista toma el tile de una celda. Cada trazo se
  deshace de una vez.
- **Sólidos**: con "Mark solids", un clic en la paleta marca o desmarca un tile como
  sólido. "Show solids" resalta en la vista las celdas que frenan. Una capa con `collision`
  desmarcado es pura decoración.
- El tamaño del mapa (`cells`) se cambia en el Inspector, al pulsar Enter.
- Para una capa nueva: **+ New**, luego **Add component > Tilemap**, y elige la imagen del
  tileset en el campo `tileset`.

Los mapas se arman **solo en este editor**: no hay archivo de mapa aparte ni editor externo.
Las celdas se guardan dentro del `.scene.json`, una fila por línea (`-1` = vacía), así que
el mapa se lee en el archivo y un diff muestra solo las filas que cambiaron.

### Física y motor de plataformas

- **`RigidBody2D`**: la entidad se mueve sola con su `velocity` y su `gravity` (px/s²; 0 para
  juegos cenitales o de naves). Sin él, una entidad es estática.
- **`BoxCollider`**: caja de colisión centrada en el `Transform`. `size` y `offset` van en
  píxeles de la **imagen**, y se multiplican por la escala del `Transform` (como el sprite);
  el offset se refleja cuando el sprite está volteado. Sin `RigidBody2D` es un obstáculo
  fijo; con `isTrigger` no frena ni es frenado; con `oneWay` es una **plataforma de un
  sentido** (solo frena a lo que cae desde arriba).
- Los cuerpos chocan con las celdas sólidas de los tilemaps y entre sí. Contra los tiles se
  resuelve **eje por eje** (así no se engancha en las costuras del suelo) y una sonda de
  2 px bajo los pies decide si está apoyado.
- **`PlatformerMotor`**: correr y saltar con buena sensación: aceleración y frenado, altura
  del salto **en píxeles**, salto corto al soltar el botón, *coyote time*, *jump buffer*,
  saltos en el aire (`maxAirJumps = 1` = doble salto) y bajar de las plataformas de un
  sentido (abajo + salto). Todas sus perillas están en el Inspector: tocarlas mientras se
  juega es el mejor ejercicio para entender el *game feel*.
- El motor no lee el teclado: lo hace un sistema del juego (`game/PlatformerControl`) para
  las entidades con tag `Player`. Flechas o `A`/`D` para moverse; `Espacio`, `W` o flecha
  arriba para saltar; abajo (`↓` o `S`) + salto para bajar de una plataforma de un sentido.
- La casilla **Colliders** de la barra dibuja las cajas en la vista Scene (verde: sólida;
  amarillo: trigger).

Los sistemas corren en el orden en que `main.cpp` los agrega, y ese orden es la cadena de
cada frame: control (teclas → intención) → `PlatformerMotorSystem` (intención → velocidad)
→ mecanismos (plataformas móviles y que caen) → `PhysicsSystem` (velocidad → posición,
choques y apoyo) → trampolines → reglas (vida, daño, frutas, checkpoints, reaparición) →
reglas del juego y HUD → presentación (animación, sonido, cámara, fondo).

### Mecanismos

Piezas del escenario que se mueven. Las plataformas que se mueven son **cinemáticas** (sin
`RigidBody2D`): llevan consigo a quien está parado encima.

- **`PathMover`**: recorre una lista de puntos (relativos a su posición inicial) a una
  velocidad, con una pausa en cada punto; en circuito o de ida y vuelta. El recorrido se
  dibuja en la vista Scene.
- **`FallingPlatform`**: cae un momento después de que la pisan y vuelve al rato, con un
  fundido.
- **`Bouncer`**: un trampolín: lanza a quien cae encima desde arriba a una altura en
  píxeles.

### Prefabs

Un **prefab** es una entidad molde guardada en su propio archivo (`assets/prefabs/*.prefab.json`):
el jugador, una fruta, unos pinchos. Ocupan el lugar que en un motor "a código" tendría una
función que arma cada objeto.

- **Colocar**: doble clic en el panel Prefabs (aparece en el centro de la vista) o
  arrastrarlo a la vista.
- Una **instancia** queda **enlazada** a su prefab: en la escena solo se guarda en qué
  difiere de él (la posición, el nombre, la textura de *otra* fruta...). Cambiar el prefab
  cambia todas sus instancias, salvo en lo que cada una sobrescribió. En el Inspector, los
  campos sobrescritos se ven en **naranja**.
- **Create prefab...** (Inspector de una entidad suelta): la guarda como prefab y la enlaza.
- **Apply** (aplicar): los cambios de la instancia pasan al prefab (y a todas sus
  instancias), salvo la posición y el nombre. El archivo del prefab se guarda en el acto:
  **Undo** vuelve atrás la escena, no el prefab.
- **Revert** (revertir): la instancia vuelve a ser igual al prefab (conserva posición y
  nombre).
- **Unlink** (desvincular): deja de ser instancia; conserva sus componentes.
- **Refresh** (panel Prefabs) vuelve a leer los prefabs del disco, si se editaron a mano.

En el archivo de escena, una instancia se ve así:

```json
{ "id": 4, "prefab": "assets/prefabs/fruit.prefab.json", "components": {
    "EntityInfo": {"name":"Banana"},
    "Transform": {"position":[576.0,384.0]},
    "Sprite": {"texture":"assets/pixel_adventure/Items/Fruits/Bananas.png"}
}}
```

### Reglas de juego

Componentes de datos y sistemas del motor que funcionan por **contacto entre cajas**
(`BoxCollider`, normalmente trigger) y deciden a quién afectan por la **etiqueta** (`tag`):

- **`Health`**: vida, invulnerabilidad tras un golpe (el sprite parpadea) y muerte.
- **`Hazard`**: quita vida al que lo toca y lo empuja (pinchos, sierras).
- **`KillZone`**: mata en el acto, aunque sea invulnerable (la franja bajo el nivel).
- **`Collectible`**: se recoge al tocarlo (frutas, monedas); desaparece en el acto o, como
  las frutas del ejemplo, al terminar su animación de recogida.
- **`Checkpoint`**: fija el punto de reaparición de quien lo toca (su bandera se anima con
  una regla `activated`).
- **`Respawn`**: en el jugador; al morir espera un momento y reaparece en el último
  checkpoint con la vida llena.

No hay *callbacks*: lo que pasa en un frame queda como un aviso de un frame en el componente
(`justDamaged`, `justDied`, `justCollected`...). Las reglas propias de **un** juego van en
`game/`: en el ejemplo, `PlatformerRules` cuenta las frutas recogidas, registra el
componente `LevelGoal` (la meta) y escribe el HUD.

### Presentación

- **`SpriteAnimator`**: una lista de **clips**, cada uno una secuencia de cuadros de una
  imagen en grilla (una tira por archivo, una fila o columna de una hoja, o celdas sueltas).
  Por clip: `fps`, `loop` y, si no repite, qué pasa al terminar: seguir con otro clip
  (`next`) o destruir la entidad (`destroyOnEnd`, para efectos). No dibuja: escribe la
  imagen y el recorte del `Sprite`. Un clip sin `texture` usa la imagen que tenía el
  `Sprite`: así la fruta de cada instancia sigue siendo un *override* del `Sprite`.
- **`AnimatorStateMachine`**: elige el clip con **reglas** "clip si condición", en orden; gana
  la primera que se cumpla y, si ninguna, suena `defaultClip`. Las condiciones son funciones
  de C++ con nombre (ver más abajo).
- **`FollowCamera`** (en la entidad de la `Camera`): sigue a la entidad con `targetTag` con
  zona muerta, suavizado, adelanto hacia donde corre (`lookAhead`) y sin salirse de los
  tilemaps. Elegida, la vista Scene muestra su zona muerta.
- **`ParallaxBackground`**: imagen en mosaico que llena la vista y acompaña a la cámara solo
  en parte (`factor`), con desplazamiento propio opcional (`scroll`).
- **`Text`**: una línea de texto con una fuente TTF, en el mundo o, con `screenSpace`, en la
  pantalla del juego (el **HUD**). En la vista Scene el HUD se ve sobre el recuadro de la
  cámara y se arrastra como cualquier entidad. El juego solo cambia el contenido; dónde va,
  la fuente y el color se eligen en el editor.
- **Pixel art nítido a cualquier tamaño**: con la escala **Pixel perfect** (la de siempre
  en la pestaña Game y en `--play`), un píxel del dibujo (`Camera.pixelSize`) mide siempre
  un número entero de píxeles de pantalla; se ve un poco más o menos de mundo. Con **Fit**,
  el juego se dibuja a su resolución y se agranda con vecino más cercano al entero de
  arriba y después se ajusta con suavizado (`GameCanvas`). Escalar directo a un factor no
  entero deforma los píxeles (unos de 3 px, otros de 4) y las animaciones parecen saltar.

### Audio

- **`SoundEffects`**: una lista de reglas "suena *este archivo* cuando *esta condición*
  empieza a cumplirse", con su volumen. Vale igual para un suceso (`jumped`, `damaged`) que
  para un estado (`dead`, `collected`): suena una vez, al empezar.
- **`Music`**: la música de fondo de la escena (archivo, volumen, loop).
- Al detener el juego se corta el sonido; en pausa se congela. En el Inspector, el botón
  **`>`** junto a un archivo de audio lo hace sonar.
- Formatos: WAV, OGG Vorbis y Opus (SDL3_mixer). Los sonidos y la música del ejemplo
  (`assets/audio/`) se generaron por síntesis para el proyecto.

### Condiciones

Las reglas de animación y de sonido eligen su condición por **nombre** en el Inspector; cada
condición es una función de C++. El motor trae estados (`grounded`, `airborne`, `rising`,
`falling`, `moving`, `dead`, `hurt`, `collected`, `activated`, `crumbling`) y sucesos de un
frame (`jumped`, `landed`, `dropped`, `damaged`, `respawned`, `bounced`), y el juego suma
las suyas (el ejemplo agrega `goalReached`):

```cpp
app.conditions().add("attacking", [](World& w, Entity e) { /* ... */ });
```

### Escenas de ejemplo

**Open...** (barra superior) lista las escenas de `assets/`:

- `platformer.scene.json`: un nivel de plataformas completo con Pixel Adventure, armado
  con prefabs: jugador, frutas, checkpoints, pinchos, sierras (fijas y móviles),
  plataformas de un sentido, móviles y que caen, un trampolín, la meta y una zona de
  muerte bajo los pozos; fondo con parallax, cámara que sigue (con zoom 1,5), HUD de vida
  y frutas, efectos de sonido y música.
- `topdown.scene.json`: un piso de Ninja Adventure, para un juego cenital.
- `shmup.scene.json`: un fondo largo de Pixel Shmup, que se recorre hacia arriba.

| Acción | Control |
|---|---|
| Elegir / arrastrar una entidad | Clic izquierdo (con `Alt`, sin ajustar a la grilla) |
| Mover la vista | Clic derecho o medio arrastrando |
| Zoom hacia el cursor | Rueda del ratón |
| Centrar en la seleccionada | `F` (o doble clic en la Hierarchy) |
| Elegir varias | `Ctrl+clic` (sumar / quitar) o `Mayús+clic` (rango, en la Hierarchy) |
| Anidar / reordenar entidades | Arrastrar en la Hierarchy (al medio: hija; al borde: antes o después) |
| Agrupar la selección | `Ctrl+G` |
| Jugar / detener | `Ctrl+P` |
| Guardar | `Ctrl+S` |
| Deshacer / rehacer | `Ctrl+Z` / `Ctrl+Y` o `Ctrl+Shift+Z` |
| Duplicar / borrar | `Ctrl+D` / `Supr` |
| Tilemap: mover / pincel / goma / relleno | `V` / `B` / `E` / `G` |
| Tilemap: tomar el tile de una celda | `Alt+clic` |
| Captura de pantalla (a `screenshots/`) | `F9` |

Con el argumento `--play`, el ejecutable abre la escena y la juega directamente, sin editor
(`--scale=pixel|fit|100` elige la escala).

## Estructura

```
engine/
  core/        Entity, World (ECS), Reflection, SceneSerializer, Prefabs, Hierarchy
  components/  componentes del motor (EntityInfo, Transform, Sprite, Tilemap, Camera,
               RigidBody2D, BoxCollider, PlatformerMotor, Health, Hazard, KillZone,
               Collectible, Checkpoint, Respawn, PathMover, FallingPlatform, Bouncer,
               SpriteAnimator, AnimatorStateMachine, FollowCamera, ParallaxBackground,
               Text, SoundEffects, Music)
  systems/     interfaz System, RenderSystem, PhysicsSystem, PlatformerMotorSystem,
               mecanismos (Motion), sistemas de reglas (Rules), de animación (Animation),
               FollowCamera y Parallax (Presentation), sonido y música (AudioSystems),
               condiciones con nombre (Conditions) y consultas de Tilemaps
  platform/    Input, AssetManager, Screenshot, Audio (SDL3_mixer), GameCanvas
  app/         Application: ventana, bucle, modos Edit / Play / Paused; GameScreen (escala)
  editor/      el editor (Dear ImGui)
  third_party/ nlohmann/json y Dear ImGui (vendorizadas)
game/          código del juego de ejemplo (PlatformerControl: teclas -> intención;
               PlatformerRules: frutas, meta, HUD y la condición goalReached)
assets/        sprites, tilesets, audio, prefabs (assets/prefabs/) y escenas (assets/scenes/)
docs/          GUIA_CODIGO_PROPIO.md: cómo sumar componentes y sistemas propios
main.cpp       registra los sistemas en orden y abre la escena
```

## Cómo compilar y ejecutar

Un único proyecto de **Visual Studio 2026** (`sdl-game-editor.vcxproj`, sin `.sln`).

### Requisitos

- **Visual Studio 2026** con el toolset de C++ (`v145`), C++17, plataforma **x64**.
- La familia **SDL3 instalada a mano** (no viene de vcpkg), en estas versiones y rutas:

  | Librería | Versión | Ruta esperada | Para qué |
  |---|---|---|---|
  | **SDL3** | **3.4.14** | `D:\SDL3` | ventana, renderer, input, tiempo |
  | **SDL3_image** | **3.4.4** | `D:\SDL3_image` | cargar PNG |
  | **SDL3_ttf** | **3.2.2** | `D:\SDL3_ttf` | texto |
  | **SDL3_mixer** | **3.2.4** | `D:\SDL3_mixer` | audio |

  Cada una se descomprime (paquete de desarrollo para VC) en su carpeta de `D:\`, de modo que
  queden `<carpeta>\include` y `<carpeta>\lib\x64`. Si usas otras rutas, ajusta
  `AdditionalIncludeDirectories`, `AdditionalLibraryDirectories` y el `PostBuildEvent` del
  `.vcxproj`.
- **nlohmann/json** y **Dear ImGui** ya vienen en `engine/third_party/`: no hay nada que
  instalar.

### Pasos

1. Abre `sdl-game-editor.vcxproj` en Visual Studio 2026.
2. Elige **x64** (sirven **Debug** y **Release**; Win32 no está soportado).
3. Compila y ejecuta (F5). El post-build copia las DLLs de SDL3 y la carpeta `assets/`
   junto al ejecutable.

Desde Visual Studio el directorio de trabajo es la carpeta del proyecto, así que **Save**
escribe en los `assets/` del repo. Si ejecutas el `.exe` desde otra carpeta, guarda en la
copia de `assets/` que está a su lado.

## Créditos y licencias de terceros

### Librerías

- **[nlohmann/json](https://github.com/nlohmann/json)** de **Niels Lohmann** — JSON para
  escenas y prefabs. Licencia **MIT**, vendorizada en `engine/third_party/nlohmann/json.hpp`.
- **[Dear ImGui](https://github.com/ocornut/imgui)** de **Omar Cornut** y colaboradores —
  interfaz del editor. Licencia **MIT**, vendorizada (v1.92.9b) en
  `engine/third_party/imgui/`, con su `LICENSE.txt` y un `VENDORED.txt` con el origen.

### Assets

- **[Pixel Adventure](https://pixelfrog-assets.itch.io/pixel-adventure-1)** de **Pixel Frog**
  (itch.io). Respeta su licencia si reutilizas o redistribuyes los assets.
- **[Ninja Adventure Asset Pack](https://pixel-boy.itch.io/ninja-adventure-asset-pack)** de
  **Pixel-boy y AAA**. **CC0** (dominio público; atribución no obligatoria pero apreciada).
- **[Pixel Shmup](https://kenney.nl/assets/pixel-shmup)** de **Kenney**. **CC0**. Ver
  `assets/kenney_pixelshmup/License.txt`.
- **Audio** (`assets/audio/`): efectos y música generados por síntesis para este proyecto,
  sin licencia de terceros.

Se usan con fines educativos.

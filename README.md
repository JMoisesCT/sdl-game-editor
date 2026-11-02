# SDL Game Editor — editor y motor 2D educativo sobre SDL3

Motor de videojuegos 2D con **editor integrado**, construido sobre SDL3 en C++17. Su
propósito es **educativo**: que estudiantes que ya conocen SDL3 básico armen juegos
editando escenas en un editor propio, sin pelear con la API cruda de SDL.

> **Estado:** en desarrollo, por fases. Ya están el núcleo (ECS + reflexión + escenas en
> JSON), el editor y los tilemaps (pintar con paleta, importar de Tiled). Faltan la física y
> los sistemas de juego: hoy una escena se puede editar, guardar y "jugar", pero todavía no
> hay nada que se mueva solo.

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
  edita. Un componente nuevo aparece en el editor **sin tocar el editor**.
- **La aplicación es dueña del bucle.** El juego no escribe su `main` de SDL: registra sus
  componentes y sistemas, y abre una escena.

  ```cpp
  int main(int argc, char* argv[]) {
      Application app;
      if (!app.init({ "Mi juego", 1280, 720 })) return 1;
      app.registry().add<MiComponente>("MiComponente").field(/* ... */);
      app.addSystem(std::make_unique<MiSistema>());
      app.openScene("assets/scenes/nivel1.scene.json");
      return app.run();
  }
  ```

## El editor

- **Jerarquía** (izquierda): las entidades de la escena; crear, duplicar y borrar.
- **Inspector** (derecha): los componentes de la entidad elegida, editables; agregar y
  quitar componentes.
- **Vista** (centro), con dos pestañas:
  - **Escena**: cámara libre del editor, grilla, selección y arrastre.
  - **Juego**: lo que ve el jugador, a la resolución del juego.
- **Jugar / Pausa / Detener**: al detener, la escena vuelve exactamente a como estaba antes
  de jugar.

### Tilemaps

Un nivel de tiles se arma con entidades que tienen el componente **`Tilemap`**: cada una es
una **capa** (fondo, suelo, decoración...). Como cada capa tiene su propio `order`, se
intercala con los sprites: el jugador puede quedar delante del suelo y detrás de unas
plantas. Su `Transform` marca la **esquina superior izquierda** del mapa (no el centro, como
en los sprites).

- Elige una capa: el Inspector muestra la **paleta** del tileset. Clic en un tile para
  elegirlo, o arrastra para elegir un bloque de varios.
- Herramientas: **Pincel** (`B`), **Goma** (`E`), **Relleno** (`G`) y **Mover** (`V`).
  `Alt+clic` en la vista toma el tile de una celda. Cada trazo se deshace de una vez.
- **Sólidos**: con "Marcar solidos", un clic en la paleta marca o desmarca un tile como
  sólido. "Ver solidos" resalta en la vista las celdas que frenan. Una capa con `collision`
  desmarcado es pura decoración.
- El tamaño del mapa (`cells`) se cambia en el Inspector, al pulsar Enter.
- **Importar Tiled...** (barra superior) convierte un mapa de [Tiled](https://www.mapeditor.org/)
  (`.json`, capas en CSV y tileset incrustado) en capas `Tilemap`. Se importa **una vez**:
  después el mapa se edita aquí y se guarda en la escena.

En el `.scene.json` las celdas se guardan una fila por línea (`-1` = vacía), así que el
mapa se lee en el archivo y un diff muestra solo las filas que cambiaron.

| Acción | Control |
|---|---|
| Elegir / arrastrar una entidad | Clic izquierdo (con `Alt`, sin ajustar a la grilla) |
| Mover la vista | Clic derecho o medio arrastrando |
| Zoom hacia el cursor | Rueda del ratón |
| Centrar en la seleccionada | `F` (o doble clic en la Jerarquía) |
| Jugar / detener | `Ctrl+P` |
| Guardar | `Ctrl+S` |
| Deshacer / rehacer | `Ctrl+Z` / `Ctrl+Y` o `Ctrl+Shift+Z` |
| Duplicar / borrar | `Ctrl+D` / `Supr` |
| Tilemap: mover / pincel / goma / relleno | `V` / `B` / `E` / `G` |
| Tilemap: tomar el tile de una celda | `Alt+clic` |
| Captura de pantalla (a `screenshots/`) | `F9` |

Con el argumento `--play`, el ejecutable abre la escena y la juega directamente, sin editor.

## Estructura

```
engine/
  core/        Entity, World (ECS), Reflection, SceneSerializer
  components/  componentes del motor (EntityInfo, Transform, Sprite, Tilemap, Camera)
  systems/     interfaz System, RenderSystem y consultas de Tilemaps
  platform/    Input, AssetManager, Screenshot, TiledImport
  app/         Application: ventana, bucle, modos Edit / Play / Pausa
  editor/      el editor (Dear ImGui)
  third_party/ nlohmann/json y Dear ImGui (vendorizadas)
assets/        sprites, mapas y escenas (assets/scenes/*.scene.json)
main.cpp
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
  | **SDL3_mixer** | **3.2.4** | `D:\SDL3_mixer` | audio (enlazado, aún sin usar) |

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

Desde Visual Studio el directorio de trabajo es la carpeta del proyecto, así que **Guardar**
escribe en los `assets/` del repo. Si ejecutas el `.exe` desde otra carpeta, guarda en la
copia de `assets/` que está a su lado.

## Créditos y licencias de terceros

### Librerías

- **[nlohmann/json](https://github.com/nlohmann/json)** de **Niels Lohmann** — JSON para
  escenas y mapas. Licencia **MIT**, vendorizada en `engine/third_party/nlohmann/json.hpp`.
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

Se usan con fines educativos.

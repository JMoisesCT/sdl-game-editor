# Guía: agregar tu propio código

Esta guía explica cómo sumar **comportamiento nuevo** a un juego hecho con el editor, de
modo que sus valores se puedan **ver y cambiar en el Inspector**, se **guarden en la
escena** y funcionen al pulsar **Jugar**. No hace falta tocar nada de `engine/` ni del
editor: todo va en `game/` y en `main.cpp`.

## La idea en 30 segundos

El motor es un **ECS**: entidades, componentes y sistemas.

| Pieza | Qué es | Dónde va |
|---|---|---|
| **Componente** | Un `struct` con **datos**, sin lógica (`velocidad`, `daño`...). | `game/MiCosa.h` |
| **Registro** | La lista de campos que el editor muestra y la escena guarda. | `game/MiCosa.cpp` |
| **Sistema** | La **lógica**: cada frame recorre las entidades con ciertos componentes y las cambia. | `game/MiCosa.cpp` |
| **Alta** | Registrar el componente y agregar el sistema al arrancar. | `main.cpp` |

Lo que **registras** aparece solo en el Inspector y en "Agregar componente": el editor no
conoce ningún componente en particular, lee la lista de campos.

## Ejemplo completo: un objeto que flota

Vamos a hacer `Floater`, un componente que hace subir y bajar a una entidad (una moneda,
una plataforma decorativa), con dos perillas editables: cuánto sube y qué tan rápido.

### 1. El componente y el sistema (`game/Floater.h`)

```cpp
#pragma once
#include "core/Math.h"
#include "systems/System.h"

class ComponentRegistry;

// Hace flotar a la entidad: sube y baja alrededor de donde empezo.
struct Floater {
    float amplitude = 8.0f; // px hacia arriba y hacia abajo
    float frequency = 0.5f; // subidas y bajadas por segundo

    // --- Estado de ejecucion (no se registra: ni se guarda ni se ve) ---
    bool started = false;
    Vec2 origin;            // donde estaba al empezar
    float time = 0.0f;
};

void registerFloater(ComponentRegistry& registry);

class FloaterSystem : public System {
public:
    const char* name() const override { return "Floater"; }
    void update(World& world, float dt) override;
};
```

- Los **valores por defecto** (`= 8.0f`) son los que tiene el componente al agregarlo desde
  el editor, y los que toma un campo que falta en el archivo.
- Los campos de abajo son **estado de ejecución**: los usa el sistema mientras se juega,
  pero no son "configuración". Por eso no se registran.

### 2. El registro y la lógica (`game/Floater.cpp`)

```cpp
#include "Floater.h"

#include <cmath>

#include "components/CoreComponents.h"
#include "core/Reflection.h"
#include "core/World.h"

void registerFloater(ComponentRegistry& registry) {
    registry.add<Floater>("Floater")
        .field("amplitude", &Floater::amplitude).range(0, 200).tooltip("Px que sube y baja")
        .field("frequency", &Floater::frequency).range(0, 10).speed(0.05f).tooltip("Vueltas por segundo");
}

void FloaterSystem::update(World& world, float dt) {
    world.each<Floater, Transform>([dt](Entity, Floater& f, Transform& t) {
        if (!f.started) { // la primera vez: recordar donde estaba
            f.started = true;
            f.origin = t.position;
            f.time = 0.0f;
        }
        f.time += dt;
        const float angle = f.time * f.frequency * 6.2831853f; // 2 pi por vuelta
        t.position.y = f.origin.y + std::sin(angle) * f.amplitude;
    });
}
```

- `registry.add<Floater>("Floater")`: el texto es el **nombre** con el que se ve en el
  editor y se guarda en el `.scene.json`. No lo cambies después de usarlo en escenas: las
  escenas viejas no lo encontrarían.
- `world.each<Floater, Transform>(...)` recorre **solo** las entidades que tienen los dos
  componentes. Pon primero el más raro (el que menos entidades tienen).
- `dt` son los segundos desde el frame anterior: multiplica por `dt` todo lo que sea "por
  segundo".

### 3. Darlo de alta (`main.cpp`)

```cpp
#include "game/Floater.h"

// ... despues de app.init(config):
registerFloater(app.registry());      // ANTES de app.openScene

// ... junto a los demas sistemas, en su lugar de la cadena:
app.addSystem(std::make_unique<FloaterSystem>());
```

- El registro va **antes de `openScene`**. Si no, al cargar la escena el componente
  todavía no existe y se descarta (el log dice `componente desconocido 'Floater'`).
- Los sistemas corren **en el orden en que se agregan**. Mira los comentarios de
  `main.cpp`: la cadena es control → motor → mecanismos → física → reglas → juego →
  presentación. Un sistema que mueve cosas sin física (como este) va con los mecanismos,
  antes de `PhysicsSystem`. Uno que **lee** lo que hizo otro, va **después** de ese otro.

### 4. Sumar los archivos al proyecto

En Visual Studio: clic derecho en el proyecto → **Agregar → Elemento existente...** y
elegir `Floater.h` y `Floater.cpp` (o crearlos desde **Agregar → Nuevo elemento**, que ya
los suma). Un `.cpp` que no está en el proyecto no se compila y el enlazador se queja de
`registerFloater` o de `FloaterSystem`.

### 5. Usarlo en el editor

1. Compila y ejecuta.
2. Elige una entidad, **Agregar componente → Floater**.
3. Pulsa **Jugar** (`Ctrl+P`): la entidad flota. Cambia `amplitude` en el Inspector
   **mientras juegas** para ver el efecto en el acto.
4. **Detener** devuelve la escena a como estaba (los cambios hechos jugando se pierden: es
   a propósito). Cambia los valores editando y guarda con `Ctrl+S`.

## Tipos de campo

Un campo registrado puede ser de estos tipos (cualquier otro no compila, con un mensaje que
lo dice):

| Tipo en C++ | En el Inspector | En el archivo |
|---|---|---|
| `bool` | casilla | `true` |
| `int` | número arrastrable | `3` |
| `float` | número arrastrable | `0.5` |
| `std::string` | texto (o lista desplegable con `.choices`) | `"Player"` |
| `Vec2` | dos números | `[16.0, 32.0]` |
| `Color` | selector de color | `[255, 255, 255, 255]` |
| `IntRect` | cuatro enteros | `[0, 0, 32, 32]` |
| `std::vector<int>` | `6, 7, 8` | `[6, 7, 8]` |
| `IntGrid` | solo el tamaño | una fila por línea |
| `Entity` | lista de entidades de la escena | el id: `12` (`0` = ninguna) |
| `std::vector<MiStruct>` (con `.list`) | lista plegable con subir/bajar/duplicar/quitar | un elemento por línea |

## Opciones del registro

Después de cada `.field(...)` se pueden encadenar opciones, que afectan al **último**
campo agregado:

```cpp
registry.add<Turret>("Turret")
    .notRemovable()                                    // (va primero) el editor no ofrece quitarlo
    .field("range", &Turret::range).range(0, 800)      // limita el valor
    .field("rate", &Turret::rate).speed(0.05f)         // cuanto cambia al arrastrar
        .tooltip("Disparos por segundo")               // ayuda al pasar el raton
    .field("icon", &Turret::icon).texture()            // ruta de imagen: selector con vista previa
    .field("shot", &Turret::shot).audio()              // ruta de sonido: selector con boton ">"
    .field("mode", &Turret::mode).choices({ "fijo", "girando" })   // lista desplegable
    .list("waves", &Turret::waves, [](FieldsBuilder<Wave>& w) {    // lista de structs
        w.field("count", &Wave::count).field("delay", &Wave::delay);
    });
```

- `.choices` también acepta una función `(const World&, Entity) -> std::vector<std::string>`,
  para opciones que dependen de la entidad (así ofrece el `SpriteAnimator` los nombres de
  sus clips).
- Las **listas** (y las grillas) conviene registrarlas **al final** del componente: en el
  archivo ocupan varias líneas.

## Estado de ejecución

Todo lo que el sistema necesita recordar mientras se juega (un temporizador, la posición
inicial, "ya empezó") va en el componente **sin registrar**:

- Al pulsar **Jugar**, la escena se copia; al **Detener**, se restaura. Lo no registrado
  vuelve a su valor por defecto.
- Inicialízalo de forma **perezosa**, como `started` en `Floater`: así vale también para
  entidades creadas mientras se juega (una bala, un enemigo que aparece).
- Si el **sistema** (no el componente) guarda estado, por ejemplo un contador de la partida,
  reinícialo en `onPlay(World&)`, que se llama al pulsar Jugar.

## Referencias a otras entidades (campo `Entity`)

Un campo de tipo `Entity` apunta a **otra entidad de la escena**: la puerta que abre un
botón, el objetivo de una torreta. En el Inspector se elige de una lista, o **arrastrando**
la entidad desde la Jerarquía y soltándola sobre el campo.

```cpp
// game/Follower.h
struct Follower {
    Entity target = NullEntity; // a quien sigue; NullEntity = a nadie
    float speed = 120.0f;       // px por segundo
    float stopDistance = 4.0f;  // px: mas cerca que esto, se queda quieto

    // --- Estado de ejecucion ---
    bool arrived = false;
    bool justArrived = false;   // aviso de un frame: llego en ESTE frame
};

// game/Follower.cpp
void registerFollower(ComponentRegistry& registry) {
    registry.add<Follower>("Follower")
        .field("target", &Follower::target).tooltip("La entidad a seguir")
        .field("speed", &Follower::speed).range(0, 1000)
        .field("stopDistance", &Follower::stopDistance).range(0, 100);
}

void FollowerSystem::update(World& world, float dt) {
    world.each<Follower, Transform>([&](Entity, Follower& f, Transform& t) {
        f.justArrived = false; // el aviso dura un solo frame

        const Transform* goal = world.get<Transform>(f.target);
        if (!goal) return; // no tiene objetivo, o el objetivo ya no existe

        float dx = goal->position.x - t.position.x;
        float dy = goal->position.y - t.position.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        if (distance <= f.stopDistance) {
            if (!f.arrived) f.justArrived = true;
            f.arrived = true;
            return;
        }
        f.arrived = false;
        float step = std::fmin(f.speed * dt, distance - f.stopDistance);
        t.position.x += dx / distance * step;
        t.position.y += dy / distance * step;
    });
}
```

- La referencia puede quedar apuntando a una entidad **que ya no existe** (se destruyó
  jugando, o se borró en el editor). No pasa nada malo: `world.get<T>(e)` devuelve
  `nullptr` y `world.isAlive(e)` devuelve `false`. Los ids no se reutilizan, así que nunca
  apunta "por error" a otra.
- **Duplicar** una rama de la Jerarquía hace que las referencias **dentro** de la rama
  apunten a las copias; las que apuntan **afuera** se mantienen.
- En un **prefab** las referencias quedan vacías (un id solo tiene sentido dentro de una
  escena): cada instancia elige la suya, y se ve en naranja como cualquier *override*.

## Comunicar sistemas: avisos de un frame y condiciones

No hay *callbacks*. Lo que **pasa** en un frame se deja en el componente como un **aviso**
(`justArrived`, como `justLanded` o `justCollected` del motor): el sistema dueño lo apaga
al empezar su `update` y lo enciende cuando corresponde. Los sistemas que corren
**después** en el mismo frame lo ven.

Para que el aviso o un estado se use en las **animaciones** (`AnimatorStateMachine`) y los
**sonidos** (`SoundEffects`) desde el editor, regístralo como **condición** con nombre:

```cpp
void registerFollowerConditions(ConditionRegistry& conditions) {
    conditions.add("arrived", [](World& w, Entity e) {
        const Follower* f = w.get<Follower>(e);
        return f && f->arrived;
    });
}
// en main.cpp, antes de openScene:
registerFollowerConditions(app.conditions());
```

Con eso, `arrived` aparece en las listas desplegables de las reglas de animación y de
sonido.

## Otras herramientas útiles

- **Teclado y ratón**: `#include "platform/Input.h"` y
  `Input::isDown(Key::Space)`, `Input::wasPressed(Key::E)`,
  `Input::axis(Key::Left, Key::Right)` (−1, 0 o 1). Ver `game/PlatformerControl.cpp`.
- **Filtrar por etiqueta**: `world.get<EntityInfo>(e)->tag == "Player"`. Nunca por el nombre,
  que es para las personas.
- **Crear entidades jugando** desde un prefab:
  `Entity bala = app.prefabs().instantiate(world, "assets/prefabs/bullet.prefab.json");`
  (el sistema necesita una referencia a la `PrefabLibrary`, que puede recibir en su
  constructor).
- **Destruir**: `world.destroy(e)`. Es diferido: la entidad desaparece al final del frame,
  así que se puede llamar dentro de un `each`.
- **La jerarquía** (`#include "core/Hierarchy.h"`): `parentOf(world, e)`,
  `childrenOf(world, grupo)`, `descendantsOf(world, e)` y `destroyWithChildren(world, e)`. Es
  una jerarquía de **organización**: jugando, los hijos **no** siguen al padre.
- **Física y reglas del motor**: `collidersOverlap`, `isStandingOn`, `damageEntity`,
  `killEntity`... (ver `systems/Physics.h` y `systems/Rules.h`).

## Reglas de oro

1. **Datos en componentes, lógica en sistemas.** Un componente no tiene métodos con
   lógica ni incluye SDL.
2. **Nunca guardes un puntero a un componente** (`Transform*`) entre frames: los
   componentes se mueven en memoria cuando se agregan otros. Guarda el `Entity` y pide el
   componente de nuevo con `world.get<T>(e)`.
3. **Dentro de un `each`** no agregues ni quites componentes de los tipos que se están
   recorriendo. Destruir con `world.destroy` sí se puede.
4. **Registra antes de `openScene`**, y agrega los sistemas en el orden correcto.
5. Puedes **agregar campos** a un componente cuando quieras: las escenas viejas cargan y el
   campo nuevo toma su valor por defecto. **Renombrar** un campo o un componente rompe lo
   ya guardado (el valor vuelve al de por defecto).

## Si algo no funciona

| Síntoma | Causa probable |
|---|---|
| El componente no aparece en "Agregar componente" | No se llamó a la función de registro, o se llamó después de `openScene`. |
| Error de enlace con `registerX` o `XSystem` | El `.cpp` no está agregado al proyecto. |
| El log dice `componente desconocido` | La escena se cargó antes del registro, o el nombre de `add<T>("...")` cambió. |
| Un campo no se ve en el Inspector | No está registrado con `.field`. |
| Un valor vuelve atrás al Detener | Es lo esperado: lo que cambia jugando no se guarda. Cámbialo editando. |
| El sistema no hace nada | No se agregó con `app.addSystem`, o la entidad no tiene **todos** los componentes del `each`. |
| No compila: "Tipo de campo no soportado" | El tipo no está en la tabla de arriba (p. ej. `double` o `short`). |
| Un número aparece como lista de entidades | Es un `unsigned` / `std::uint32_t`, que es el mismo tipo que `Entity`. Para números usa `int`. |
| El efecto llega un frame tarde | El sistema corre antes que el que produce el dato: muévelo después en `main.cpp`. |

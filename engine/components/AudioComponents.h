#pragma once
#include <string>
#include <vector>

class ComponentRegistry;
class ConditionRegistry;

// Componentes de AUDIO. Como todos, son SOLO datos: los hacen sonar SoundSystem y
// MusicSystem (systems/AudioSystems.h) a traves de platform/Audio.

// Un sonido y CUANDO suena: al EMPEZAR a cumplirse la condicion (el frame en que pasa
// de falsa a verdadera). Asi vale igual para un suceso de un frame (jumped, damaged)
// que para un estado (dead, collected): suena una vez al morir, no cada frame muerto.
struct SoundRule {
    std::string condition;   // nombre de una condicion registrada (systems/Conditions.h)
    std::string sound;       // archivo de audio
    float volume = 1.0f;     // 0 = mudo, 1 = como esta grabado

    // --- Estado de ejecucion ---
    bool wasTrue = false;    // la condicion en el frame anterior
};

// Los efectos de sonido de una entidad: una lista de reglas "suena X cuando Y", como
// las del AnimatorStateMachine pero sin prioridad (suenan todas las que empiecen).
struct SoundEffects {
    std::vector<SoundRule> sounds;

    // --- Estado de ejecucion ---
    bool started = false;    // ya se miraron las condiciones una vez (el primer frame
                             // solo se anotan: lo que ya era cierto al empezar no suena)
};

// La musica de fondo. Suena mientras haya una entidad con Music (la primera que se
// encuentre); si el juego cambia el archivo, cambia la musica.
struct Music {
    std::string music;       // archivo de audio
    float volume = 0.5f;
    bool loop = true;
};

// Declara los componentes de arriba a la reflexion. La llama Application al arrancar,
// despues de registerPresentationComponents.
void registerAudioComponents(ComponentRegistry& registry, const ConditionRegistry& conditions);

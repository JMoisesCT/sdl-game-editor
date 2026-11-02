#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct MIX_Mixer; // declaraciones adelantadas: SDL3_mixer solo aparece en el .cpp
struct MIX_Audio;
struct MIX_Track;

// El AUDIO del motor, sobre SDL3_mixer: efectos de sonido que se superponen y una
// musica de fondo. Como el AssetManager, es el DUENO de todo lo que carga (cada archivo
// se lee una sola vez y se reusa).
//
// Formatos: WAV, OGG Vorbis y Opus (las DLL de Opus las copia el post-build).
//
// Si no hay dispositivo de audio (un equipo sin parlantes, una sesion remota), init()
// devuelve false y todo lo demas no hace nada: el juego sigue, mudo.
//
// Desde un sistema del juego (aunque lo normal es el componente SoundEffects):
//     app.audio().playSound("assets/audio/sfx/jump.wav", 0.8f);
class Audio {
public:
    Audio() = default;
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init();
    void shutdown();
    bool ready() const { return mixer != nullptr; }

    // Un efecto: suena entero y se mezcla con lo que ya suene (hasta MAX_VOICES a la vez;
    // pasado eso corta el mas viejo). volume: 0 = mudo, 1 = como esta grabado.
    void playSound(const std::string& path, float volume = 1.0f);

    // La musica: una sola a la vez, con un fundido corto. Pedir la que ya se pidio solo
    // le cambia el volumen (no la reinicia; sin loop, tampoco la repite).
    void playMusic(const std::string& path, float volume = 1.0f, bool loop = true);
    void stopMusic();

    void stopAll();               // al detener el juego
    void setPaused(bool paused);  // la pausa del editor congela tambien el sonido

private:
    static const int MAX_VOICES = 16;
    MIX_Audio* load(const std::string& path);

    MIX_Mixer* mixer = nullptr;
    bool subsystemReady = false;
    std::unordered_map<std::string, MIX_Audio*> sounds; // SI somos dueno
    std::unordered_set<std::string> failed;             // rutas que no cargaron
    std::vector<MIX_Track*> voices;                     // pistas para los efectos
    int nextVoice = 0;                                  // la que se corta si estan todas ocupadas
    MIX_Track* musicTrack = nullptr;
    std::string musicPath;                              // la que suena ("" = ninguna)
};

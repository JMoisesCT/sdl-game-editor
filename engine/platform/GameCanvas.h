#pragma once

struct SDL_Renderer; // declaraciones adelantadas: SDL solo aparece en el .cpp
struct SDL_Texture;

// El LIENZO del juego: el juego se dibuja a SU resolucion (la de AppConfig) en una
// textura, y despues se muestra escalado donde haya lugar (la ventana con --play, la
// pestana "Juego" del editor).
//
// Por que no escalar directo con vecino mas cercano: a un factor NO entero, cada pixel
// ocupa en pantalla mas o menos pixeles segun donde caiga. Con 3 px por pixel de arte
// (sprites a escala 2 y camara con zoom 1,5), a 1,25x quedan columnas de 3 y de 4
// pixeles: cuando el personaje "respira" y se corre un pixel, cambia de forma y parece
// que salta. Con suavizado lineal, en
// cambio, todo se ve borroso.
//
// Escalado NITIDO (el "sharp bilinear" de los emuladores): primero se agranda con
// vecino mas cercano al entero de arriba (2x: cada pixel, un bloque exacto de 2x2) y
// ese resultado se achica con suavizado lineal hasta el tamano final. Los pixeles
// quedan todos iguales y solo el borde entre dos se mezcla en una linea.
class GameCanvas {
public:
    explicit GameCanvas(SDL_Renderer* renderer) : renderer(renderer) {}
    ~GameCanvas();
    GameCanvas(const GameCanvas&) = delete;
    GameCanvas& operator=(const GameCanvas&) = delete;

    // Deja el lienzo (width x height) como destino de render; lo crea la primera vez o
    // si cambia el tamano. false si no se pudo crear.
    bool begin(int width, int height);

    // La textura con la que mostrar el lienzo en un rectangulo de displayWidth x
    // displayHeight pixeles de pantalla (quien llama la dibuja ahi). Deja el destino de
    // render en la ventana. nullptr si no hay lienzo.
    SDL_Texture* scaledFor(float displayWidth, float displayHeight);

    int width() const { return canvasW; }
    int height() const { return canvasH; }

private:
    SDL_Renderer* renderer;
    SDL_Texture* canvas = nullptr;   // a la resolucion del juego
    int canvasW = 0, canvasH = 0;
    SDL_Texture* upscaled = nullptr; // el lienzo agrandado a un factor entero
    int upscaledFactor = 0;
};

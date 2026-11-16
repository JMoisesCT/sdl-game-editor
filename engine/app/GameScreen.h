#pragma once

class World;
class AssetManager;
class GameCanvas;
struct SDL_Renderer;  // declaraciones adelantadas: SDL solo aparece en el .cpp
struct SDL_Texture;

// Como se muestra el juego (su resolucion, p. ej. 1280x720) en un area de otro tamano:
// la pestana "Juego" del editor o la ventana con --play.
//
// El problema: el pixel art solo se ve bien si cada pixel del dibujo mide un numero
// ENTERO de pixeles de pantalla. En platformer mide 3 (escala 2 x zoom 1,5); si el
// area es un 70% del juego, pasaria a medir 2,1: con suavizado se ve borroso, y con
// vecino mas cercano, deforme (columnas de 2 y de 3 px).
enum class GameScale {
    Fit,          // "Ajustar": la imagen del juego entera, escalada para que quepa (nitida
                  // al agrandar; achicando se suaviza y el pixel art queda borroso)
    PixelPerfect, // "Pixel perfecto": se dibuja directo al tamano del area y el zoom se
                  // redondea (Camera::pixelSize) para que cada pixel del dibujo mida un
                  // entero de px. Se ve un poco mas o menos de mundo que en el juego.
    Actual,       // "100%": a su resolucion, centrado; si no cabe, se recorta
};

// Nombre para mostrar ("Ajustar", "Pixel perfecto", "100%") y para la linea de
// comandos ("fit", "pixel", "100"). parseGameScale: false si el texto no es ninguno.
const char* gameScaleLabel(GameScale scale);
bool parseGameScale(const char* text, GameScale& scale);

// Lo que hay que mostrar: 'texture' (la parte src) en el rectangulo dst, medido dentro
// del area (enteros; lo que queda alrededor son bandas negras).
struct GameScreen {
    SDL_Texture* texture = nullptr;
    float textureW = 0, textureH = 0;
    float srcX = 0, srcY = 0, srcW = 0, srcH = 0;
    float dstX = 0, dstY = 0, dstW = 0, dstH = 0;
    float scale = 0.0f;       // tamano mostrado / resolucion del juego
    float pixel = 0.0f;       // px de pantalla por pixel del dibujo (Pixel perfecto; si no, 0)
    bool hasCamera = false;
};

// Dibuja el juego (mundo + HUD) en el lienzo para un area de areaW x areaH px. Deja el
// destino de render en la ventana. false si el area esta vacia o no hay textura.
bool drawGameScreen(World& world, AssetManager& assets, SDL_Renderer* renderer, GameCanvas& canvas,
                    int gameWidth, int gameHeight, GameScale mode, float areaW, float areaH, GameScreen& out);

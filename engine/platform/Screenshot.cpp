#include "Screenshot.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstdio>

namespace Screenshot {

std::string save(SDL_Renderer* renderer, const std::string& folder) {
    if (!renderer) return "";

    // Reads what is drawn on the renderer RIGHT NOW. That is why it is called before
    // SDL_RenderPresent: after Present the backbuffer's content is not guaranteed
    // (it may come out black or as the previous frame).
    SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
    if (!shot) {
        SDL_Log("Screenshot: could not read the screen: %s", SDL_GetError());
        return "";
    }

    SDL_CreateDirectory(folder.c_str()); // if it already exists, nothing happens

    // Name with the local date and time; the milliseconds avoid overwriting a capture
    // if the key is pressed twice in the same second.
    SDL_Time now = 0;
    SDL_DateTime dt{};
    SDL_GetCurrentTime(&now);
    SDL_TimeToDateTime(now, &dt, true);
    char name[64];
    std::snprintf(name, sizeof(name), "screenshot_%04d%02d%02d_%02d%02d%02d_%03d.png",
                  dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
                  dt.nanosecond / 1000000);
    std::string path = folder + "/" + name;

    bool ok = IMG_SavePNG(shot, path.c_str());
    SDL_DestroySurface(shot);
    if (!ok) {
        SDL_Log("Screenshot: could not save %s: %s", path.c_str(), SDL_GetError());
        return "";
    }

    // The path is relative to the working directory, so the full path is written to
    // the log to know where it ended up.
    char* cwd = SDL_GetCurrentDirectory(); // ends with a separator
    SDL_Log("Screenshot: saved to %s%s", cwd ? cwd : "", path.c_str());
    SDL_free(cwd);
    return path;
}

} // namespace Screenshot

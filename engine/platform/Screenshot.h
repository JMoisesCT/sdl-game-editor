#pragma once
#include <string>

struct SDL_Renderer;

// Screenshot to PNG. It is not a component: it is a free function the main loop
// calls whenever it wants (e.g. when a key is pressed).
//
// Typical use (in the loop, with EVERYTHING already drawn and BEFORE SDL_RenderPresent):
//     if (Input::wasPressed(Key::F9)) Screenshot::save(renderer);
//
// Saves into the given folder (relative to the working directory; created if it does
// not exist) a file "screenshot_YYYYMMDD_HHMMSS_mmm.png". Returns the file's path,
// or an empty string if it failed (the reason goes to the log).

namespace Screenshot {
    std::string save(SDL_Renderer* renderer, const std::string& folder = "screenshots");
}

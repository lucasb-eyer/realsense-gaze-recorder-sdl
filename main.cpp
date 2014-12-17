#include <iostream>

#include <SDL.h>

#include <pxcsensemanager.h>

int main(int argc, char **argv){
    // Initialize RealSense
    PXCSenseManager *pp = PXCSenseManager::CreateInstance();
    if (!pp) {
        std::cerr << "Unable to create the SenseManager" << std::endl;
        return 1;
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 2;
    }

    // TODO

    SDL_Quit();
    pp->Release();
    return 0;
}
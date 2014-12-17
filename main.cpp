#include <codecvt>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <locale>
#include <string>

#include <SDL.h>

#include <pxcsensemanager.h>

// As global so we can use atexit.
PXCSenseManager *g_sm = nullptr;
SDL_Window *g_window = nullptr;
SDL_Renderer *g_renderer = nullptr;

// Makes our error-checking life a little easier.
bool pxc_verify(pxcStatus ret, std::string msg);
bool sdl_verify(int ret, std::string msg);
bool init_realsense();

int main(int argc, char **argv)
{
    if (!sdl_verify(SDL_Init(SDL_INIT_EVERYTHING), "initializing SDL"))
        return 1;
    std::atexit(SDL_Quit);

    // Gets `g_sm` ready for recording what we need.
    if (!init_realsense())
        return 2;

    // Open up a window.
    if (!sdl_verify(SDL_CreateWindowAndRenderer(0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP, &g_window, &g_renderer), "opening a window"))
        return 3;
    atexit([](){ SDL_DestroyWindow(g_window); });
    atexit([](){ SDL_DestroyRenderer(g_renderer); });

    // This is an extremely simple state-machine for handling input with the states
    // preparing -> recording -> done.
    enum {
        STATE_PRE,
        STATE_RECORDING,
        STATE_DONE,
    } state = STATE_PRE;

    // Remembers at what time the recording started.
    auto t0 = 0;

    SDL_Event e = { 0 };
    while (e.type != SDL_QUIT) {
        // Handle all events before moving to the next frame!
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_KEYUP) {
                // Start recording when the user presses a key!
                if (state == STATE_PRE) {
                    state = STATE_RECORDING;
                    t0 = SDL_GetTicks();
                }
                // When done recording, quit upon a keypress.
                else if (state == STATE_DONE) {
                    e.type = SDL_QUIT;
                }
            }
            // Ignore all other kinds of events.
        }

        // Update the dot's position according to the "storyline".
        if (state == STATE_RECORDING) {
            // For now, the storyline is to record one second!
            state = SDL_GetTicks() < t0 + 5000 ? STATE_RECORDING : STATE_DONE;
        }

        // Clear the screen in black.
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);

        // TODO: Render

        // Swap framebuffers.
        SDL_RenderPresent(g_renderer);

        // Only record when we should be recording, duh!
        if (state == STATE_RECORDING) {
            // It seems that acquiring frames is necessary for the recording to record anything!
            // I tried just idling in a MessageBox and it didn't work.

            // Waits until new frame is available and locks it for application processing.
            //std::cout << "\rCapturing frame " << nframes + 1 << std::flush;
            if (!pxc_verify(g_sm->AcquireFrame(true), "Acquiring frame"))
                return 100;  // TODO: Apparently one should recover from PXC_STATUS_STREAM_CONFIG_CHANGED?

            // Done working with the frame.
            g_sm->ReleaseFrame();
        }
    }

    return 0;
}

bool pxc_verify(pxcStatus ret, std::string msg)
{
    if (ret < PXC_STATUS_NO_ERROR) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RealSense Error", ("RealSense error #" + std::to_string(ret) + ": " + msg).c_str(), nullptr);
        return false;
    }
    if (ret > PXC_STATUS_NO_ERROR)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RealSense Error", ("RealSense warning #" + std::to_string(ret) + ": " + msg).c_str(), nullptr);
    return true;
}

bool sdl_verify(int ret, std::string msg)
{
    if (ret != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Error", ("Error " + msg + ": " + SDL_GetError()).c_str(), nullptr);
        return false;
    }
    return true;
}

// Current date and time as almost-ISO string.
std::string now()
{
    std::time_t rawtime;
    std::time(&rawtime);

    char buffer[80];
    std::strftime(buffer, 80, "%Y-%m-%d-%H-%M-%S", std::localtime(&rawtime));
    return std::string(buffer);
}

bool init_realsense()
{
    // Initialize RealSense
    if ((g_sm = PXCSenseManager::CreateInstance()) == nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RealSense Error", "Unable to create the SenseManager.", nullptr);
        return false;
    }
    std::atexit([](){ g_sm->Release(); });

    // Sets file recording or playback
    char *pszPath = SDL_GetPrefPath("Beymans", "RealSenseRecorder");
    if (!pszPath) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Error", "Can't retrieve your home directory. What the!?", nullptr);
        return false;
    }
    std::string utf8path = pszPath + now() + ".rssdk";
    SDL_free(pszPath);
    std::cout << "Recording to " << utf8path << std::endl;

    // Damn you RealSense!
    std::wstring path = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(utf8path);
    if (!pxc_verify(g_sm->QueryCaptureManager()->SetFileName(path.c_str(), true), "Setting filename for recording."))
        return false;

    // Chooses what streams we want to capture.
    if (!pxc_verify(g_sm->EnableStream(PXCCapture::STREAM_TYPE_COLOR, 640, 480, 30), "Enabling RGB stream."))
        return false;
    if (!pxc_verify(g_sm->EnableStream(PXCCapture::STREAM_TYPE_DEPTH, 640, 480, 30), "Enabling D stream. Yup Alex, can't get the D!"))
        return false;

    return pxc_verify(g_sm->Init(), "Initialize the capture.");
}

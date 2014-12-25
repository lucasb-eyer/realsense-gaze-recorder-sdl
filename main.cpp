#include <codecvt>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <locale>
#include <memory>
#include <string>
#include <thread>

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include <pxcsensemanager.h>

// Some cleanup helpers.
auto surf_deleter = [](SDL_Surface* s){ SDL_FreeSurface(s); };
auto tex_deleter = [](SDL_Texture* t){ SDL_DestroyTexture(t); };

// As global so we can use atexit.
PXCSenseManager *g_sm = nullptr;
SDL_Window *g_window = nullptr;
SDL_Renderer *g_renderer = nullptr;
TTF_Font *g_font = nullptr;

enum {
    TEXT_INSTRUCTION = 0,
    TEXT_START,
    TEXT_QUIT,
    TEXT_FILE,
    TEXT_COUNT
};
std::unique_ptr<SDL_Texture, decltype(tex_deleter)> g_texts[TEXT_COUNT] = { nullptr };

// Makes our error-checking life a little easier.
bool pxc_verify(pxcStatus ret, std::string msg);
bool sdl_verify(int ret, std::string msg);
bool ttf_verify(int ret, std::string msg);
bool init_realsense();
std::unique_ptr<SDL_Texture, decltype(tex_deleter)> mktxt(const char* txt);

// x,y are relative screen coordinates, 0 being top/left and 1 being bottom/right.
// w,h are screen resolution.
void rendermid(SDL_Texture* tex, double x, double y, int w, int h);

double lerp(double t, double x0, double x1, double t0, double t1) { return x0 + (t - t0) / (t1 - t0) * (x1 - x0); };

int main(int argc, char **argv)
{
    if (!sdl_verify(SDL_Init(SDL_INIT_EVERYTHING), "initializing SDL"))
        return 1;
    std::atexit(SDL_Quit);

    if (!ttf_verify(TTF_Init(), "initializing SDL_TTF"))
        return 1;
    std::atexit(TTF_Quit);

    bool isInitPngSet = (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == IMG_INIT_PNG;
    // Be aware that SDL uses 0 for ok.
    if (!sdl_verify(isInitPngSet ? 0 : -1 , "initializing SDL Image"))
        return 1;

    // Gets `g_sm` ready for recording what we need.
    if (!init_realsense())
        return 2;

    // Open up a window.
#ifdef _DEBUG
    if (!sdl_verify(SDL_CreateWindowAndRenderer(640, 480, 0, &g_window, &g_renderer), "opening a window"))
#else
    if (!sdl_verify(SDL_CreateWindowAndRenderer(0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP, &g_window, &g_renderer), "opening a window"))
#endif
        return 3;
    atexit([](){ SDL_DestroyWindow(g_window); });
    atexit([](){ SDL_DestroyRenderer(g_renderer); });

    // Get the window's w/h.
    int w, h;
    SDL_GL_GetDrawableSize(g_window, &w, &h);

    // Load a default font.
    if (!ttf_verify((g_font = TTF_OpenFont("data/Orbitron Medium.ttf", 24)) == nullptr, "opening the Orbitron font"))
        return 4;
    atexit([](){ TTF_CloseFont(g_font); });

    g_texts[TEXT_INSTRUCTION] = mktxt("Follow the green dot with your eyes.");
    g_texts[TEXT_START] = mktxt("Press any key to start.");
    g_texts[TEXT_QUIT] = mktxt("Press any key to quit.");
    g_texts[TEXT_FILE] = mktxt("Recording into the ~User/AppData/Roaming/...");
    for (int i = 0; i < TEXT_COUNT; ++i)
        if (g_texts[i] == nullptr)
            return 5;

    // Load Mr.Point into a texture.
    SDL_Surface* mrpoint_surf = IMG_Load("data/mrpoint.png");
    if (!sdl_verify(mrpoint_surf == nullptr, "loading Mr.Point"))
        return 6;
    std::unique_ptr<SDL_Texture, decltype(tex_deleter)> mrpoint(
        SDL_CreateTextureFromSurface(g_renderer, mrpoint_surf),
        tex_deleter
    );
    SDL_FreeSurface(mrpoint_surf);
    if (!sdl_verify(mrpoint == nullptr, "loading Mr.Point texture"))
        return 6;

    // This is an extremely simple state-machine for handling input with the states
    // preparing -> recording -> done.
    enum State {
        STATE_PRE,
        STATE_RECORDING,
        STATE_DONE,
        STATE_QUIT,
    } state = STATE_PRE;

    // Remembers at what time the recording started.
    Uint32 t0 = 0;

    // A separate thread for recording, otherwise we're LAGGY.
    std::thread record_thread;

    SDL_Event e = { 0 };
    while (state != STATE_QUIT) {
        // Handle all events before moving to the next frame!
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_KEYUP) {
                // Start recording when the user presses a key!
                if (state == STATE_PRE) {
                    state = STATE_RECORDING;
                    t0 = SDL_GetTicks();
                    
                    // Start the other thread which will record the video.
                    record_thread = std::move(std::thread([](State* pstate){
                        // Only record when we should be recording, duh!
                        while (*pstate == STATE_RECORDING) {
                            // It seems that acquiring frames is necessary for the recording to record anything!
                            // I tried just idling in a MessageBox and it didn't work.

                            // Waits until new frame is available and locks it for application processing.
                            if (!pxc_verify(g_sm->AcquireFrame(true), "Acquiring frame"))
                                break;  // TODO: Apparently one should recover from PXC_STATUS_STREAM_CONFIG_CHANGED?

                            // Done working with the frame.
                            g_sm->ReleaseFrame();
                        }
                    }, &state));
                }
                // When done recording, quit upon a keypress.
                else if (state == STATE_DONE) {
                    state = STATE_QUIT;
                    break;
                }
            }
            else if (e.type == SDL_QUIT) {
                state = STATE_QUIT;
                break;
            }
            // Ignore all other kinds of events.
        }

        // The current position of Mr.Point (in relative screen-coordinates).
        double x = 0.01, y = 0.01;

        // Update the dot's position according to the "storyline".
        if (state == STATE_RECORDING) {
            double t = 0.001*(SDL_GetTicks() - t0);

            // That's the choreography!
            if (0 <= t && t < 3) {
                x = lerp(t, 0.01, 0.99, 0, 3);
            }
            else if (3 <= t && t < 5) {
                y = lerp(t, 0.01, 0.99, 3, 5);
            }
            else if (5 <= t && t < 8) {
                x = lerp(t, 0.99, 0.01, 5, 8);
            }
            else if (8 <= t && t < 10) {
                y = lerp(t, 0.99, 0.01, 8, 10);
            }
            else {
                // Switch over to done state.
                state = STATE_DONE;
                record_thread.join();
            }
        }

        // Clear the screen in black.
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
        SDL_RenderClear(g_renderer);

        // The rendering.
        switch (state) {
        case STATE_PRE:
            rendermid(g_texts[TEXT_INSTRUCTION].get(), 0.5f, 0.33f, w, h);
            rendermid(g_texts[TEXT_START].get(), 0.5f, 0.66f, w, h);
            rendermid(mrpoint.get(), x, y, w, h);
            break;
        case STATE_RECORDING:
            rendermid(mrpoint.get(), x, y, w, h);
            break;
        case STATE_DONE:
            rendermid(g_texts[TEXT_QUIT].get(), 0.5f, 0.5f, w, h);
            break;
        }

        // Swap framebuffers.
        SDL_RenderPresent(g_renderer);
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

bool ttf_verify(int ret, std::string msg)
{
    if (ret != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Error", ("Error " + msg + ": " + TTF_GetError()).c_str(), nullptr);
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

std::unique_ptr<SDL_Texture, decltype(tex_deleter)> mktxt(const char* txt)
{
    SDL_Color white = { 255, 255, 255, 255 };

    //We need to first render to a surface as that's what TTF_RenderText
    //returns, then load that surface into a texture
    std::unique_ptr<SDL_Surface, decltype(surf_deleter)> surf(
        TTF_RenderText_Blended(g_font, txt, white),
        surf_deleter
    );

    if (!ttf_verify(surf == nullptr, "writing some text."))
        return nullptr;

    std::unique_ptr<SDL_Texture, decltype(tex_deleter)> tex(
        SDL_CreateTextureFromSurface(g_renderer, surf.get()),
        tex_deleter
    );
    sdl_verify(tex == nullptr, "moving the surface to a texture.");
    return tex;
};

void rendermid(SDL_Texture* tex, double x, double y, int w, int h)
{
    // Get the texture w/h.
    int tw, th;
    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);

    //Setup the destination rectangle to be at the (pixel) position we want.
    SDL_Rect dst;
    dst.x = int(x*w - tw*0.5);
    dst.y = int(y*h - th*0.5);

    //Query the texture to get its width and height to use
    SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);
    SDL_RenderCopy(g_renderer, tex, NULL, &dst);
}

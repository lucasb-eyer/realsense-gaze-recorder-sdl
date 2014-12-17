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

// Makes our error-checking life a little easier.
bool pxc_verify(pxcStatus ret, std::string msg);
// Current date and time as almost-ISO string.
std::string now();

int main(int argc, char **argv){
    // Initialize SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    std::atexit(SDL_Quit);

    // Initialize RealSense
    if ((g_sm = PXCSenseManager::CreateInstance()) == nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RealSense Error", "Unable to create the SenseManager.", nullptr);
        return 2;
    }
    std::atexit([](){ g_sm->Release(); });

    // Sets file recording or playback
    char *pszPath = SDL_GetPrefPath("Beymans", "RealSenseRecorder");
    if (!pszPath) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Error", "Can't retrieve your home directory. What the!?", nullptr);
        return 100;
    }
    std::string utf8path = pszPath + now() + ".rssdk";
    SDL_free(pszPath);
    std::cout << "Recording to " << utf8path << std::endl;

    // Damn you RealSense!
    std::wstring path = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(utf8path);
    if (!pxc_verify(g_sm->QueryCaptureManager()->SetFileName(path.c_str(), true), "Setting filename for recording."))
        return 3;

    // Chooses what streams we want to capture.
    if (!pxc_verify(g_sm->EnableStream(PXCCapture::STREAM_TYPE_COLOR, 640, 480, 30), "Enabling RGB stream."))
        return 4;
    if (!pxc_verify(g_sm->EnableStream(PXCCapture::STREAM_TYPE_DEPTH, 640, 480, 30), "Enabling D stream. Yup Alex, can't get the D!"))
        return 4;

    if (!pxc_verify(g_sm->Init(), "Initialize the capture."))
        return 5;

    // It seems that acquiring frames is necessary for the recording to record anything!
    // I tried just idling in a MessageBox and it didn't work.
    // TODO: Or maybe there's another "pull"-like API for recording without explicitly waiting?
    for (int nframes = 0; nframes<100; nframes++) {
        std::cout << "\rCapturing frame " << nframes+1 << std::flush;

        // Waits until new frame is available and locks it for application processing.
        if (!pxc_verify(g_sm->AcquireFrame(true), "Acquiring frame"))
            return 6;  // TODO: Apparently one should recover from PXC_STATUS_STREAM_CONFIG_CHANGED?

        // Done working with the frame.
        g_sm->ReleaseFrame();
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

std::string now()
{
    std::time_t rawtime;
    std::time(&rawtime);

    char buffer[80];
    std::strftime(buffer, 80, "%Y-%m-%d-%H-%M-%S", std::localtime(&rawtime));
    return std::string(buffer);
}

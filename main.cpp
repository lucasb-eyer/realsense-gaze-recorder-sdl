#include <iostream>
#include <string>
#include <cstdlib>

#include <SDL.h>

#include <pxcsensemanager.h>

// As global so we can use atexit.
PXCSenseManager *g_sm = nullptr;

// Makes our error-checking life a little easier.
bool pxc_verify(pxcStatus ret, std::string msg);

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
    if (!pxc_verify(g_sm->QueryCaptureManager()->SetFileName(L"C:/Users/beyer/lolz.rssdk", true), "Setting filename for recording."))
        return 3;

    // Chooses what streams we want to capture.
    if (!pxc_verify(g_sm->EnableStream(PXCCapture::STREAM_TYPE_COLOR, 640, 480, 30), "Enabling RGB stream."))
        return 4;
    if (!pxc_verify(g_sm->EnableStream(PXCCapture::STREAM_TYPE_DEPTH, 640, 480, 30), "Enabling D stream. Yup Alex, can't get the D!"))
        return 5;

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

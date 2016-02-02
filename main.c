#define __USE_MISC
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "libjpeg-turbo/turbojpeg.h"
#include "net.h"

static SDL_Texture* screen_texture = NULL;
SDL_Texture* alloc_texture(SDL_Renderer* rdr, int width, int height)
{
    static int curw=0, curh=0;
    if(screen_texture)
    {
        if(curw == width && curh == height)
            return screen_texture;
        curw = width;
        curh = height;
        SDL_DestroyTexture(screen_texture);
    }
    return screen_texture = SDL_CreateTexture(rdr, SDL_PIXELFORMAT_ARGB8888,
                                   SDL_TEXTUREACCESS_STREAMING, width, height);
}

SDL_Texture* decode_frame(SDL_Renderer* rdr, void* data, int length, int* _width, int* _height)
{
    static tjhandle tjd = NULL;
    if(!tjd)
        tjd = tjInitDecompress();

    int width, height, jpegSubsamp, jpegColorspace;
    if(tjDecompressHeader3(tjd, (unsigned char*)data, (unsigned long)length,
                        &width, &height,
                        &jpegSubsamp, &jpegColorspace))
    {
        fprintf(stderr, "decode_frame: failed to parse jpeg");
        return NULL;
    }
    SDL_Texture* tex = alloc_texture(rdr, width, height);
    void* pixels = NULL;
    int pitch = -1;
    if(SDL_LockTexture(tex, NULL, &pixels, &pitch))
    {
        fprintf(stderr, "decode_frame: failed to lock texture");
        return NULL;
    }
    if(tjDecompress2(tjd, (unsigned char*)data, (unsigned long)length, (unsigned char*)pixels,
                  width, pitch, height, TJPF_ARGB, 0))
    {
        fprintf(stderr, "decode_frame: failed to decode jpeg");
        return NULL;
    }
    SDL_UnlockTexture(tex);
    if(_width)
        *_width = width;
    if(_height)
        *_height = height;
    return tex;
}

int main(int argc, char** argv)
{
    setlinebuf(stdout);
    setlinebuf(stderr);

    if(argc != 2)
    {
        fprintf(stderr, "usage: %s ipaddr\n", argv[0]);
        return 1;
    }

    if(do_connect(argv[1]) < 0)
    {
        fprintf(stderr, "error: failed to connect to %s:1920\n", argv[1]);
        return 2;
    }

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* win = NULL;
    SDL_Renderer* rdr = NULL;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    if(SDL_CreateWindowAndRenderer(640, 480,
                                   SDL_WINDOW_RESIZABLE|SDL_WINDOW_MAXIMIZED,
                                   &win, &rdr))
    {
        printf("window creation failed\n");
        return 1;
    }

    bool runnning = true;
    SDL_Event e;
    while(runnning)
    {
        switch(poll_frame())
        {
        case -1:
            break;
        case 1:{
            void* data = NULL;
            int len = get_jpeg_data(&data);
            if(len > 0)
            {
                int w, h;
                if(decode_frame(rdr, data, len, &w, &h))
                {
                    static Uint32 last = 0;
                    Uint32 now = SDL_GetTicks();
                    /*char tmp[512];
                    snprintf(tmp, 511, "%.2f fps\n", 1000.0/(SDL_GetTicks()-last));
                    SDL_SetWindowTitle(win, tmp);*/
                    printf("main: frame at %.2f fps\n", 1000.0/(now-last));
                    last = now;
                    SDL_RenderSetLogicalSize(rdr, w, h);
                    //SDL_SetWindowSize(win, w, h);

                    /*SDL_Event e;
                    e.type = SDL_WINDOWEVENT;
                    e.window.event = SDL_WINDOWEVENT_EXPOSED;
                    e.window.windowID = SDL_GetWindowID(win);
                    e.window.timestamp = SDL_GetTicks();
                    SDL_PushEvent(&e);*/
                }
            }
        }
        }

        while(SDL_WaitEventTimeout(&e, 5))
        {
            switch(e.type)
            {
            case SDL_QUIT:
                runnning = false;
                break;
            case SDL_WINDOWEVENT:
                if(e.window.event == SDL_WINDOWEVENT_CLOSE)
                    runnning = false;
                /*else if(e.window.event == SDL_WINDOWEVENT_EXPOSED)
                    if(screen_texture)
                    {
                        SDL_RenderClear(rdr);
                        SDL_RenderCopy(rdr, screen_texture, NULL, NULL);
                        SDL_RenderPresent(rdr);
                    }*/
                break;
            }
        }
        if(screen_texture)
        {
            SDL_RenderClear(rdr);
            SDL_RenderCopy(rdr, screen_texture, NULL, NULL);
            SDL_RenderPresent(rdr);
        }
    }

    SDL_Quit();

    return 0;
}


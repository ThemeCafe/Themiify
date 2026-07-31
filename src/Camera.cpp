/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Camera.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>

using std::cout;
using std::cerr;
using std::endl;

namespace Camera {
    static constexpr int WIDTH  = CAMERA_WIDTH;
    static constexpr int HEIGHT = CAMERA_HEIGHT;
    static constexpr int PITCH  = CAMERA_PITCH;

    static SDL_Renderer* gRenderer = nullptr;
    static SDL_Texture* gTexture = nullptr;

    static CAMHandle gCamera = -1;
    static CAMSurface gSurface {};

    static void* gWorkMem = nullptr;
    static void* gSurfaceBuffer = nullptr;

    static uint32_t* gRGBBuffer = nullptr;

    static std::atomic<bool> gInitialized(false);
    static std::atomic<bool> gHasNewFrame(false);
    static std::atomic<void*> gLatestYUV(nullptr);
    static std::atomic<const uint8_t*> gLatestYPlane(nullptr);

    static FlipMode gFlipMode = FlipMode::horizontal;

    static inline uint8_t clamp_u8(int v) {
        return static_cast<uint8_t>(std::clamp(v, 0, 255));
    }

    static void convert_nv12_to_argb8888(const uint8_t* yuv, uint32_t* out) {
        const uint8_t* yPlane  = yuv;
        const uint8_t* uvPlane = yuv + CAMERA_Y_BUFFER_SIZE;

        for (int y = 0; y < HEIGHT; y++) {
            const uint8_t* yRow  = yPlane + y * PITCH;
            const uint8_t* uvRow = uvPlane + (y / 2) * PITCH;

            for (int x = 0; x < WIDTH; x++) {
                int Y = yRow[x];

                int uvIndex = (x / 2) * 2;
                int U = uvRow[uvIndex + 0];
                int V = uvRow[uvIndex + 1];

                int C = Y - 16;
                int D = U - 128;
                int E = V - 128;

                int R = (298 * C + 409 * E + 128) >> 8;
                int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
                int B = (298 * C + 516 * D + 128) >> 8;

                uint8_t r = clamp_u8(R);
                uint8_t g = clamp_u8(G);
                uint8_t b = clamp_u8(B);

                uint32_t pixel =
                    (0xFFu << 24) |
                    (static_cast<uint32_t>(r) << 16) |
                    (static_cast<uint32_t>(g) << 8) |
                    static_cast<uint32_t>(b);

                int dstX = x;
                int dstY = y;

                switch (gFlipMode) {
                    case FlipMode::none:
                        break;

                    case FlipMode::horizontal:
                        dstX = WIDTH - 1 - x;
                        break;

                    case FlipMode::vertical:
                        dstY = HEIGHT - 1 - y;
                        break;

                    case FlipMode::both:
                        dstX = WIDTH - 1 - x;
                        dstY = HEIGHT - 1 - y;
                        break;
                }

                out[dstY * WIDTH + dstX] = pixel;
            }
        }
    }

    void camera_event_handler(CAMEventData* eventData) {
        if (!eventData)
            return;

        if (eventData->eventType == CAMERA_DECODE_DONE) {
            if (!eventData->decode.failed && eventData->decode.surfaceBuffer) {
                DCInvalidateRange(
                    eventData->decode.surfaceBuffer,
                    CAMERA_YUV_BUFFER_SIZE
                );

                auto* yuv = static_cast<uint8_t*>(eventData->decode.surfaceBuffer);

                gLatestYUV.store(yuv);
                gLatestYPlane.store(yuv); // Y plane starts at beginning of NV12 buffer

                gHasNewFrame.store(true);
            }

            CAMSubmitTargetSurface(eventData->decode.handle, &gSurface);
        }
    }

    bool initialize(SDL_Renderer* renderer, FlipMode flipMode) {
        if (gInitialized.load())
            return true;

        if (!renderer)
            return false;

        gRenderer = renderer;
        gFlipMode = flipMode;

        gTexture = SDL_CreateTexture(
            gRenderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            WIDTH,
            HEIGHT
        );

        if (!gTexture) {
            cerr << "SDL_CreateTexture failed: " << SDL_GetError() << endl;
            return false;
        }

        gRGBBuffer = new uint32_t[WIDTH * HEIGHT];

        CAMStreamInfo streamInfo {};
        streamInfo.type = CAMERA_STREAM_TYPE_1;
        streamInfo.width = WIDTH;
        streamInfo.height = HEIGHT;

        int32_t workMemSize = CAMGetMemReq(&streamInfo);

        if (workMemSize <= 0)
            return false;

        gWorkMem = MEMAllocFromDefaultHeapEx(
            workMemSize,
            CAMERA_YUV_BUFFER_ALIGNMENT
        );

        if (!gWorkMem)
            return false;

        std::memset(gWorkMem, 0, workMemSize);

        CAMSetupInfo setup {};
        setup.streamInfo = streamInfo;
        setup.workMem.size = workMemSize;
        setup.workMem.pMem = gWorkMem;
        setup.eventHandler = camera_event_handler;
        setup.mode.forceDrc = FALSE;
        setup.mode.fps = CAMERA_FPS_30;
        setup.threadAffinity = OS_THREAD_ATTRIB_AFFINITY_ANY;

        CAMError camErr = CAMERA_ERROR_OK;
        gCamera = CAMInit(0, &setup, &camErr);

        if (gCamera < 0 || camErr != CAMERA_ERROR_OK)
            return false;

        gSurfaceBuffer = MEMAllocFromDefaultHeapEx(
            CAMERA_YUV_BUFFER_SIZE,
            CAMERA_YUV_BUFFER_ALIGNMENT
        );

        if (!gSurfaceBuffer)
            return false;

        std::memset(gSurfaceBuffer, 0, CAMERA_YUV_BUFFER_SIZE);

        gSurface.surfaceSize = CAMERA_YUV_BUFFER_SIZE;
        gSurface.surfaceBuffer = gSurfaceBuffer;
        gSurface.height = HEIGHT;
        gSurface.width = WIDTH;
        gSurface.pitch = PITCH;
        gSurface.alignment = CAMERA_YUV_BUFFER_ALIGNMENT;
        gSurface.tileMode = 0;
        gSurface.pixelFormat = 0;

        CAMSubmitTargetSurface(gCamera, &gSurface);

        gInitialized.store(true);
        return true;
    }

    bool open() {
        return CAMOpen(gCamera) == CAMERA_ERROR_OK;
    }

    void close() {
        if (gCamera >= 0)
            CAMClose(gCamera);

        gHasNewFrame.store(false);
        gLatestYUV.store(nullptr);
    }

    void update_texture() {
        if (!gInitialized.load() || !gTexture || !gRGBBuffer)
            return;

        if (!gHasNewFrame.exchange(false))
            return;

        const uint8_t* yuv = static_cast<const uint8_t*>(gLatestYUV.load());

        if (!yuv)
            return;

        convert_nv12_to_argb8888(yuv, gRGBBuffer);

        SDL_UpdateTexture(
            gTexture,
            nullptr,
            gRGBBuffer,
            WIDTH * sizeof(uint32_t)
        );
    }

    SDL_Texture* get_texture() {
        return gTexture;
    }

    bool is_initialized() {
        return gInitialized.load();
    }

    bool is_open() {
        return gCamera >= 0;
    }

    void shutdown() {
        close();

        if (gCamera >= 0) {
            CAMExit(gCamera);
            gCamera = -1;
        }

        if (gSurfaceBuffer) {
            MEMFreeToDefaultHeap(gSurfaceBuffer);
            gSurfaceBuffer = nullptr;
        }

        if (gWorkMem) {
            MEMFreeToDefaultHeap(gWorkMem);
            gWorkMem = nullptr;
        }

        delete[] gRGBBuffer;
        gRGBBuffer = nullptr;

        if (gTexture) {
            SDL_DestroyTexture(gTexture);
            gTexture = nullptr;
        }

        gRenderer = nullptr;

        gHasNewFrame.store(false);
        gLatestYUV.store(nullptr);
        gInitialized.store(false);
    }

    const uint8_t* get_grayscale_buffer() {
        return gLatestYPlane.load();
    }

    int get_width() {
        return WIDTH;
    }

    int get_height() {
        return HEIGHT;
    }

    int get_pitch() {
        return PITCH;
    }
}

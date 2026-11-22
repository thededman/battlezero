// Linux framebuffer output (/dev/fb0 on the CardputerZero's ST7789 panel).
#pragma once
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "raster.h"

struct FbOut {
    int fd = -1;
    fb_var_screeninfo var{};
    fb_fix_screeninfo fix{};
    uint8_t* map = nullptr;
    size_t mapLen = 0;
    std::vector<uint8_t> staging; // used when the device cannot be mapped

    bool open() {
        const char* path = std::getenv("BATTLEZERO_FB");
        if (!path) path = "/dev/fb0";
        fd = ::open(path, O_RDWR);
        if (fd < 0) {
            std::perror(path);
            return false;
        }
        if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
            std::perror("framebuffer info");
            return false;
        }
        if (var.bits_per_pixel != 16 && var.bits_per_pixel != 32) {
            std::fprintf(stderr, "unsupported framebuffer depth: %u bpp\n", var.bits_per_pixel);
            return false;
        }
        mapLen = static_cast<size_t>(fix.line_length) * var.yres;
        void* m = mmap(nullptr, mapLen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (m == MAP_FAILED) staging.resize(mapLen);
        else map = static_cast<uint8_t*>(m);
        std::fprintf(stderr, "framebuffer %ux%u, %u bpp, stride %u, %s\n", var.xres, var.yres,
                     var.bits_per_pixel, fix.line_length, map ? "mapped" : "write()");
        return true;
    }

    // Copies the game frame, centred if the panel is a different size.
    void present(const bz::Frame& f) {
        uint8_t* dst = map ? map : staging.data();
        int w = static_cast<int>(var.xres), h = static_cast<int>(var.yres);
        int ox = (w - bz::SCREEN_W) / 2, oy = (h - bz::SCREEN_H) / 2;
        for (int y = 0; y < bz::SCREEN_H; ++y) {
            int dy = y + oy;
            if (dy < 0 || dy >= h) continue;
            uint8_t* row = dst + static_cast<size_t>(dy) * fix.line_length;
            int x0 = ox < 0 ? -ox : 0, x1 = bz::SCREEN_W;
            if (ox + x1 > w) x1 = w - ox;
            const bz::Color* src = &f.px[y * bz::SCREEN_W];
            if (var.bits_per_pixel == 16) {
                std::memcpy(row + static_cast<size_t>(ox + x0) * 2, src + x0, static_cast<size_t>(x1 - x0) * 2);
            } else {
                uint32_t* out = reinterpret_cast<uint32_t*>(row) + ox;
                for (int x = x0; x < x1; ++x) {
                    bz::Color c = src[x];
                    uint32_t r = ((c >> 11) & 31) * 255 / 31, g = ((c >> 5) & 63) * 255 / 63, b = (c & 31) * 255 / 31;
                    out[x] = (r << var.red.offset) | (g << var.green.offset) | (b << var.blue.offset);
                }
            }
        }
        if (!map) {
            if (pwrite(fd, staging.data(), mapLen, 0) < 0) std::perror("framebuffer write");
        }
    }

    void close() {
        if (map) {
            std::memset(map, 0, mapLen);
            munmap(map, mapLen);
            map = nullptr;
        }
        if (fd >= 0) ::close(fd);
        fd = -1;
    }
};

// Keyboard through evdev. The built-in keyboard is a TCA8418 on I2C; any other keyboard
// that is plugged in works too.
#pragma once
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "input.h"

struct Keys {
    std::vector<int> fds;
    bool down[KEY_MAX + 1] = {};

    static bool isKeyboard(int fd) {
        unsigned long bits[(KEY_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof bits), bits) < 0) return false;
        auto has = [&](int k) { return (bits[k / (8 * sizeof(unsigned long))] >> (k % (8 * sizeof(unsigned long)))) & 1; };
        return has(KEY_SPACE) && has(KEY_F) && has(KEY_X);
    }

    void add(const std::string& path) {
        int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) return;
        if (!isKeyboard(fd)) {
            ::close(fd);
            return;
        }
        // Take the keyboard for ourselves so key presses do not also reach the launcher.
        if (ioctl(fd, EVIOCGRAB, 1) < 0) std::perror("keyboard grab");
        std::fprintf(stderr, "keyboard: %s\n", path.c_str());
        fds.push_back(fd);
    }

    bool open() {
        if (const char* forced = std::getenv("BATTLEZERO_KEYS")) {
            add(forced);
            return !fds.empty();
        }
        DIR* d = opendir("/dev/input");
        if (d) {
            while (dirent* e = readdir(d)) {
                std::string n = e->d_name;
                if (n.rfind("event", 0) == 0) add("/dev/input/" + n);
            }
            closedir(d);
        }
        if (fds.empty()) std::fprintf(stderr, "no readable keyboard found under /dev/input\n");
        return !fds.empty();
    }

    void poll(bz::Input& in) {
        input_event ev[32];
        for (int fd : fds) {
            for (;;) {
                ssize_t n = read(fd, ev, sizeof ev);
                if (n <= 0) break;
                for (size_t i = 0; i < static_cast<size_t>(n) / sizeof(input_event); ++i)
                    if (ev[i].type == EV_KEY && ev[i].code <= KEY_MAX) down[ev[i].code] = ev[i].value != 0;
            }
        }
        in.left = down[KEY_Z] || down[KEY_LEFT];
        in.right = down[KEY_C] || down[KEY_RIGHT];
        in.forward = down[KEY_F] || down[KEY_UP];
        in.reverse = down[KEY_X] || down[KEY_DOWN];
        in.fire = down[KEY_SPACE] || down[KEY_ENTER];
        in.pause = down[KEY_ESC];
        in.recenter = down[KEY_R];
        in.tiltToggle = down[KEY_T];
        in.invert = down[KEY_I];
        in.axis = down[KEY_A];
    }

    void close() {
        for (int fd : fds) {
            ioctl(fd, EVIOCGRAB, 0);
            ::close(fd);
        }
        fds.clear();
    }
};

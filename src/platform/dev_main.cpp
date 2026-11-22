// BattleZero on the CardputerZero.
#include <sys/stat.h>
#include <time.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "dev_fb.h"
#include "dev_imu.h"
#include "dev_keys.h"
#include "game.h"

using namespace bz;

static volatile sig_atomic_t stopRequested = 0;
static void onSignal(int) { stopRequested = 1; }

static double nowSeconds() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

static std::string homeDir() {
    const char* h = std::getenv("HOME");
    return h && *h ? h : "/tmp";
}

// Creates each directory of a path under the home directory and returns the full path.
static std::string ensureDir(const std::string& relative) {
    std::string p = homeDir();
    size_t start = 0;
    while (start <= relative.size()) {
        size_t slash = relative.find('/', start);
        if (slash == std::string::npos) slash = relative.size();
        p += "/" + relative.substr(start, slash - start);
        mkdir(p.c_str(), 0755);
        start = slash + 1;
    }
    return p;
}

static Frame frame;

int main() {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    FbOut fb;
    Keys keys;
    Imu imu;
    if (!fb.open()) return 1;
    if (!keys.open()) {
        fb.close();
        return 1;
    }
    bool haveImu = imu.open();

    Game game(static_cast<uint32_t>(time(nullptr)));
    std::string scorePath = ensureDir(".local/share/battlezero") + "/highscore";
    std::string tiltPath = ensureDir(".config/battlezero") + "/tilt.conf";
    if (FILE* fp = std::fopen(scorePath.c_str(), "r")) {
        if (std::fscanf(fp, "%d", &game.highScore) != 1) game.highScore = 0;
        std::fclose(fp);
    }
    if (FILE* fp = std::fopen(tiltPath.c_str(), "r")) {
        int axis = 0, sign = -1;
        if (std::fscanf(fp, "%d %d", &axis, &sign) == 2 && axis >= 0 && axis <= 2 && (sign == 1 || sign == -1)) {
            game.tiltAxis = axis;
            game.tiltSign = sign;
        }
        std::fclose(fp);
    }

    const double frameTime = 1.0 / 60.0;
    double last = nowSeconds(), fpsFrom = last;
    int frames = 0;
    bool showFps = std::getenv("BATTLEZERO_FPS") != nullptr;
    char fpsText[16] = "";

    while (!stopRequested && !game.wantsQuit) {
        Input in;
        keys.poll(in);
        if (haveImu && imu.ok) {
            in.tiltAvailable = true;
            for (int i = 0; i < 3; ++i) in.accel[i] = imu.value[i].load();
        }

        double now = nowSeconds();
        game.update(in, static_cast<float>(now - last));
        last = now;

        game.render(frame);
        if (showFps) {
            if (++frames, now - fpsFrom >= 1.0) {
                std::snprintf(fpsText, sizeof fpsText, "%d FPS", frames);
                frames = 0;
                fpsFrom = now;
            }
            frame.text(SCREEN_W - 44, 160, fpsText, WHITE);
        }
        fb.present(frame);

        if (game.highScoreDirty) {
            if (FILE* fp = std::fopen(scorePath.c_str(), "w")) {
                std::fprintf(fp, "%d\n", game.highScore);
                std::fclose(fp);
            }
            game.highScoreDirty = false;
        }
        if (game.tiltConfigDirty) {
            if (FILE* fp = std::fopen(tiltPath.c_str(), "w")) {
                std::fprintf(fp, "%d %d\n", game.tiltAxis, game.tiltSign);
                std::fclose(fp);
            }
            game.tiltConfigDirty = false;
        }

        double spare = frameTime - (nowSeconds() - now);
        if (spare > 0) {
            timespec ts = {0, static_cast<long>(spare * 1e9)};
            nanosleep(&ts, nullptr);
        }
    }

    imu.close();
    keys.close();
    fb.close();
    return 0;
}

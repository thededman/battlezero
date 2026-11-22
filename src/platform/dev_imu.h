// The built-in BMI270 accelerometer through the kernel's IIO sysfs files. Read on its
// own thread so a slow I2C read can never stall a frame.
#pragma once
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

struct Imu {
    int fd[3] = {-1, -1, -1};
    std::atomic<float> value[3];
    std::atomic<bool> ok{false}, stop{false};
    std::thread worker;

    static std::string readText(const std::string& path) {
        std::string out;
        if (FILE* fp = std::fopen(path.c_str(), "r")) {
            char buf[64];
            if (std::fgets(buf, sizeof buf, fp)) out = buf;
            std::fclose(fp);
        }
        while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
        return out;
    }

    // Finds the IIO device that has accelerometer channels, preferring one named bmi270.
    static std::string findDevice() {
        std::string found;
        DIR* d = opendir("/sys/bus/iio/devices");
        if (!d) return found;
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.rfind("iio:device", 0) != 0) continue;
            std::string dir = "/sys/bus/iio/devices/" + n;
            if (access((dir + "/in_accel_x_raw").c_str(), R_OK) != 0) continue;
            if (readText(dir + "/name").find("bmi270") != std::string::npos) {
                found = dir;
                break;
            }
            if (found.empty()) found = dir;
        }
        closedir(d);
        return found;
    }

    bool readOnce() {
        for (int i = 0; i < 3; ++i) {
            char buf[32];
            ssize_t n = pread(fd[i], buf, sizeof buf - 1, 0);
            if (n <= 0) return false;
            buf[n] = 0;
            value[i].store(static_cast<float>(std::atof(buf)));
        }
        return true;
    }

    bool open() {
        std::string dir = findDevice();
        if (const char* forced = std::getenv("BATTLEZERO_IMU")) dir = forced;
        if (dir.empty()) {
            std::fprintf(stderr, "no motion sensor found: steering is by Z and C keys\n");
            return false;
        }
        const char* names[3] = {"/in_accel_x_raw", "/in_accel_y_raw", "/in_accel_z_raw"};
        for (int i = 0; i < 3; ++i) {
            fd[i] = ::open((dir + names[i]).c_str(), O_RDONLY);
            if (fd[i] < 0) {
                std::perror((dir + names[i]).c_str());
                return false;
            }
        }
        if (!readOnce()) {
            std::fprintf(stderr, "motion sensor at %s did not answer\n", dir.c_str());
            return false;
        }
        std::fprintf(stderr, "motion sensor: %s (%s)\n", dir.c_str(), readText(dir + "/name").c_str());
        ok = true;
        worker = std::thread([this] {
            while (!stop) {
                if (!readOnce()) ok = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        return true;
    }

    void close() {
        stop = true;
        if (worker.joinable()) worker.join();
        for (int& f : fd) {
            if (f >= 0) ::close(f);
            f = -1;
        }
    }
};

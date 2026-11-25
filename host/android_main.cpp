// The Android entry point: SDL's Java side (android/app/src/main/java/org/libsdl/app/SDLActivity.java) loads libmain.so and calls
// SDL_main on its own thread with the arguments GameActivity.getArguments() gives: the same command line as ps2run's
// (--disc <image> --window --game-dir <the app's folder> --memcard <card>). host/main.cpp is compiled with main=ps2_host_main
// (android/app/CMakeLists.txt) and runs unchanged from here.
//
// An Android app has no console: stdout and stderr go nowhere. Both are sent through a pipe to a thread that writes every
// line to logcat (tag ps2native, `adb logcat -s ps2native`) and to log.txt in the game folder, so a run on a phone can be
// read afterwards like a run on the PC.

#include <android/log.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

int ps2_host_main(int argc, char **argv);

namespace {

int g_log_fd = -1;

void copy_output(int from) {
    char buffer[4096];
    std::string line;
    for (;;) {
        const ssize_t got = read(from, buffer, sizeof buffer);
        if (got <= 0) return;
        if (g_log_fd >= 0) {
            ssize_t ignored = write(g_log_fd, buffer, static_cast<std::size_t>(got));
            (void)ignored;
        }
        for (ssize_t i = 0; i < got; ++i) {
            if (buffer[i] == '\n') {
                __android_log_write(ANDROID_LOG_INFO, "ps2native", line.c_str());
                line.clear();
            } else if (buffer[i] != '\r') {
                line += buffer[i];
            }
        }
    }
}

void capture_output(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--game-dir") == 0) {
            const std::string path = std::string(argv[i + 1]) + "/log.txt";
            g_log_fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        }
    int fds[2];
    if (pipe(fds) != 0) return;
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    dup2(fds[1], 1);
    dup2(fds[1], 2);
    close(fds[1]);
    std::thread(copy_output, fds[0]).detach();
}

}  // namespace

extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char *argv[]) {
    capture_output(argc, argv);
    std::fprintf(stderr, "[android] starting with %d arguments\n", argc - 1);
    const int code = ps2_host_main(argc, argv);
    std::fprintf(stderr, "[android] the runner returned %d\n", code);
    std::fflush(stdout);
    std::fflush(stderr);
    // The runtime keeps its state in globals and cannot start a second time in the same process: end the process here (the
    // memory card was written when the run halted), so the next start of the app begins fresh. A short wait lets the log
    // thread pass on the last lines.
    usleep(200 * 1000);
    _exit(code);
}

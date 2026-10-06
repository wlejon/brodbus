#include "brodbus/private_bus.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include <utility>

namespace brodbus {

#if defined(__linux__)

PrivateBus::PrivateBus() {
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) return;

    pid_t p = fork();
    if (p == 0) {
        close(pipe_fds[0]);
        dup2(pipe_fds[1], STDOUT_FILENO);
        close(pipe_fds[1]);

        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > 2) close(devnull);
        }

        prctl(PR_SET_PDEATHSIG, SIGTERM);

        execlp("dbus-daemon", "dbus-daemon", "--session", "--nofork", "--nopidfile",
               "--print-address=1", static_cast<char*>(nullptr));
        _exit(127);
    }

    close(pipe_fds[1]);
    if (p < 0) {
        close(pipe_fds[0]);
        return;
    }

    pid_ = p;
    char buf[512];
    ssize_t n = read(pipe_fds[0], buf, sizeof(buf) - 1);
    close(pipe_fds[0]);
    if (n > 0) {
        buf[n] = '\0';
        std::string line = buf;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        address_ = line;
    } else {
        stop();
    }
}

PrivateBus::~PrivateBus() {
    stop();
}

PrivateBus::PrivateBus(PrivateBus&& other) noexcept
    : address_(std::move(other.address_)), pid_(other.pid_) {
    other.pid_ = -1;
}

PrivateBus& PrivateBus::operator=(PrivateBus&& other) noexcept {
    if (this != &other) {
        stop();
        address_ = std::move(other.address_);
        pid_ = other.pid_;
        other.pid_ = -1;
    }
    return *this;
}

void PrivateBus::stop() {
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        int status = 0;
        waitpid(pid_, &status, 0);
        pid_ = -1;
        address_.clear();
    }
}

#endif  // __linux__

}  // namespace brodbus

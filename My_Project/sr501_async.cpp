#include "sr501_async.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Sr501Async {

namespace {

bool setNonBlocking(int fd)
{
    int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0) {
        return false;
    }

    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

} // namespace

bool createSignalPipe(int pipeFds[2])
{
    if (!pipeFds) {
        return false;
    }

    pipeFds[0] = -1;
    pipeFds[1] = -1;

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, pipeFds) != 0) {
        return false;
    }

    if (!setNonBlocking(pipeFds[0]) || !setNonBlocking(pipeFds[1])) {
        closeSignalPipe(pipeFds);
        return false;
    }

    return true;
}

void closeSignalPipe(int pipeFds[2])
{
    if (!pipeFds) {
        return;
    }

    if (pipeFds[0] >= 0) {
        ::close(pipeFds[0]);
        pipeFds[0] = -1;
    }

    if (pipeFds[1] >= 0) {
        ::close(pipeFds[1]);
        pipeFds[1] = -1;
    }
}

bool configureAsyncNotification(int sr501Fd, pid_t ownerPid)
{
    if (sr501Fd < 0) {
        return false;
    }

    if (::fcntl(sr501Fd, F_SETOWN, ownerPid) < 0) {
        return false;
    }

    int flags = ::fcntl(sr501Fd, F_GETFL);
    if (flags < 0) {
        return false;
    }

    return ::fcntl(sr501Fd, F_SETFL, flags | O_ASYNC) == 0;
}

bool drainSignalPipe(int readFd)
{
    if (readFd < 0) {
        return false;
    }

    bool drained = false;
    char buffer[32];

    while (true) {
        ssize_t ret = ::read(readFd, buffer, sizeof(buffer));
        if (ret > 0) {
            drained = true;
            continue;
        }

        if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return drained;
        }

        return drained;
    }
}

bool readDeviceState(int sr501Fd, int &state)
{
    if (sr501Fd < 0) {
        return false;
    }

    char rawState = 0;
    // Some char drivers do not implement llseek. Keep the legacy behavior:
    // attempt to rewind, but still try read() even if lseek fails.
    (void)::lseek(sr501Fd, 0, SEEK_SET);

    if (::read(sr501Fd, &rawState, 1) != 1) {
        return false;
    }

    state = rawState != 0 ? 1 : 0;
    return true;
}

} // namespace Sr501Async

#include <cassert>
#include <cstdio>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sr501_async.h"

static void test_read_device_state_reads_latest_byte()
{
    char tempPath[] = "/tmp/sr501_asyncXXXXXX";
    int fd = ::mkstemp(tempPath);
    assert(fd >= 0);
    ::unlink(tempPath);

    char one = 1;
    assert(::write(fd, &one, 1) == 1);

    int state = -1;
    assert(Sr501Async::readDeviceState(fd, state));
    assert(state == 1);

    assert(::lseek(fd, 0, SEEK_SET) == 0);
    char zero = 0;
    assert(::write(fd, &zero, 1) == 1);

    assert(Sr501Async::readDeviceState(fd, state));
    assert(state == 0);

    ::close(fd);
}

static void test_drain_signal_pipe_consumes_all_pending_bytes()
{
    int pipeFds[2] = {-1, -1};
    assert(Sr501Async::createSignalPipe(pipeFds));

    const char payload[] = {1, 1, 1};
    assert(::write(pipeFds[0], payload, sizeof(payload)) == static_cast<ssize_t>(sizeof(payload)));

    assert(Sr501Async::drainSignalPipe(pipeFds[1]));
    assert(!Sr501Async::drainSignalPipe(pipeFds[1]));

    Sr501Async::closeSignalPipe(pipeFds);
}

static void test_configure_async_notification_sets_owner_and_flag()
{
    int socketFds[2] = {-1, -1};
    assert(::socketpair(AF_UNIX, SOCK_STREAM, 0, socketFds) == 0);

    assert(Sr501Async::configureAsyncNotification(socketFds[0], ::getpid()));
    assert(::fcntl(socketFds[0], F_GETOWN) == ::getpid());
    assert((::fcntl(socketFds[0], F_GETFL) & O_ASYNC) != 0);

    ::close(socketFds[0]);
    ::close(socketFds[1]);
}

int main()
{
    test_read_device_state_reads_latest_byte();
    test_drain_signal_pipe_consumes_all_pending_bytes();
    test_configure_async_notification_sets_owner_and_flag();
    return 0;
}

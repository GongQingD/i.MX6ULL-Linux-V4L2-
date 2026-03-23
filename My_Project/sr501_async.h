#ifndef SR501_ASYNC_H
#define SR501_ASYNC_H

#include <sys/types.h>

namespace Sr501Async {

bool createSignalPipe(int pipeFds[2]);
void closeSignalPipe(int pipeFds[2]);
bool configureAsyncNotification(int sr501Fd, pid_t ownerPid);
bool drainSignalPipe(int readFd);
bool readDeviceState(int sr501Fd, int &state);

} // namespace Sr501Async

#endif // SR501_ASYNC_H

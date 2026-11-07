#include "app/UnixSignals.h"

#include <QCoreApplication>

#ifdef Q_OS_UNIX
#include <QSocketNotifier>

#include <array>
#include <csignal>
#include <unistd.h>
#endif

namespace sheaf {

#ifdef Q_OS_UNIX

namespace {

// A signal handler may only do very little; it writes a byte into a pipe that the event loop reads.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): signal handlers can only use global state
std::array<int, 2> g_pipe = {-1, -1};

void HandleSignal(int /*signal*/) {
  const char byte = 1;
  const ssize_t written = ::write(g_pipe[1], &byte, 1);
  (void)written;
}

}  // namespace

void InstallQuitSignalHandlers() {
  if (g_pipe[0] >= 0 || ::pipe(g_pipe.data()) != 0) {
    return;
  }
  auto* notifier = new QSocketNotifier(g_pipe[0], QSocketNotifier::Read, QCoreApplication::instance());
  QObject::connect(notifier, &QSocketNotifier::activated, notifier, [] {
    char byte = 0;
    const ssize_t count = ::read(g_pipe[0], &byte, 1);
    (void)count;
    QCoreApplication::quit();
  });

  struct sigaction action{};
  action.sa_handler = HandleSignal;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESTART;
  for (const int signal_number : {SIGTERM, SIGINT, SIGHUP}) {
    sigaction(signal_number, &action, nullptr);
  }
}

#else

void InstallQuitSignalHandlers() {
}

#endif

}  // namespace sheaf

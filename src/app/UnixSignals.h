// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace sheaf {

// Makes SIGTERM, SIGINT and SIGHUP end the application the normal way (QCoreApplication::quit), so that the session
// and the open position are saved. The desktop sends SIGTERM when you log out or shut down; without this the process
// would be killed on the spot. Does nothing on platforms without POSIX signals. Call after creating the application.
void InstallQuitSignalHandlers();

}  // namespace sheaf

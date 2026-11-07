// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace sheaf {

// Makes the backend classes QML can create itself (like PdfSearchSession) available as `import SheafBackend 1.0`.
void RegisterQmlTypes();

}  // namespace sheaf

// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/QmlTypes.h"

#include "search/PdfSearchSession.h"

#include <QtQml>

namespace sheaf {

void RegisterQmlTypes() {
  qmlRegisterType<PdfSearchSession>("SheafBackend", 1, 0, "PdfSearchSession");
}

}  // namespace sheaf

# Sheaf

A minimal, fast PDF viewer for students: tabs in collapsible groups, remembered reading positions, a back/forward
history for links, fast full-text search, and session restore. Built with C++20 and Qt 6 (Qt Quick, Qt PDF), for
Linux first.

Milestones M1 to M6 of [PLAN.md](PLAN.md) are implemented (viewer, tabs and position memory, link navigation,
sidebar with groups, session restore, search).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/sheaf
./build/tests/tst_database
```

Open a PDF with Ctrl+O, or pass it on the command line: `./build/sheaf file.pdf`.

| Key                                        | Action                                                           |
|--------------------------------------------|------------------------------------------------------------------|
| Ctrl+O                                     | Open                                                             |
| Ctrl+wheel, Ctrl++, Ctrl+-                 | Zoom                                                             |
| Ctrl+0                                     | Fit width                                                        |
| Ctrl+G                                     | Go to page                                                       |
| Ctrl+F                                     | Find in the document (focuses the bar, selecting the last query) |
| Enter / F3, Shift+Enter / Shift+F3         | Next / previous match                                            |
| Esc                                        | Close the search bar and clear the highlights                    |
| Alt+Left / Alt+Right, mouse Back / Forward | Back / forward through followed links (per tab)                  |
| Ctrl+Tab / Ctrl+Shift+Tab                  | Next / previous tab (all groups, in sidebar order)               |
| Ctrl+W                                     | Close tab                                                        |
| Ctrl+O                                     | Open files into the active group                                 |
| Ctrl+Shift+N                               | New group (named right away)                                     |
| F2                                         | Rename the active group                                          |
| Ctrl+Shift+Up / Down                       | Move the active tab up / down, across groups                     |

## Sidebar

Tabs live in groups. Click a tab to show it; click a group to make it the target for new files; click the arrow to
collapse it; double-click a group to rename it; right-click for a menu (rename, move, delete, move a tab to another
group); drag tabs and groups to reorder them. With the sidebar focused: Up/Down move the cursor, Return/Space show a
tab or fold a group, Left/Right fold and unfold, F2 renames, Delete closes a tab or deletes a group. Deleting a group
closes its tabs (after asking); the position in each document is remembered.

## Search

Ctrl+F opens a search bar over the document. All matches are highlighted, the current one is framed, and the counter
reads "3 of 17". It is case-insensitive, a phrase matches across line breaks, and ligatures (ﬁ, ﬂ) do not get in the
way. A search starts from the page you are reading; the first match is in view within milliseconds, and the whole
document is counted in about a second (measured: 8354 matches over 427 pages in 0.8 s). Typing is debounced (150 ms).
Each tab keeps its own query and current match when you switch away and back. A scanned document without a
text layer says "No text in this document".

Qt's own `PdfSearchModel` is not used: it needs 20+ seconds for 400 pages and cannot report that it has finished.
`PdfSearchSession` reads the page texts with PDFium on a worker thread instead and works out the highlight rectangles
only for pages that are shown.

## Session

The groups, tabs, their order and collapsed state, the active tab, the position in each document, and the window and
sidebar size are saved as you work (a change is written within milliseconds) and again when you quit, whether by
closing the window, Ctrl+Q, or being told to end by the system (SIGTERM at logout). The next start brings it all back;
only the active document is loaded. The database is `~/.local/share/sheaf/sheaf/sheaf.db`.

A tab whose file has gone is kept and shown greyed out (⚠) with a "File not found" page offering **Locate…** (the
same file is relinked silently, a different one only after asking, and the remembered position follows) and **Remove
tab**. Opening the moved file again relinks the lost tab by itself.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

- `tst_database`, `tst_fileidentity`, `tst_positionstore`, `tst_tabcontroller`: C++ unit tests.
- `tst_qml`: QML tests in `tests/qml/` against the real QML sources with generated fixture PDFs (headless, software
  rendering): viewer behaviour (`tst_viewer.qml`), position restore without wrong or half-rendered pages
  (`tst_restore.qml`), the real window with the real tab controller (`tst_tabswitch.qml`) and link navigation
  with back/forward history (`tst_navigation.qml`), the sidebar and group shortcuts (`tst_sidebar.qml`,
  `tst_shortcuts.qml`), and quitting and restarting with missing files and window state (`tst_session.qml`).
-
    - `tst_textmatcher`, `tst_searchresults`, `tst_pdfsearchsession`: the search (matching, results and navigation, and
      the
      worker with generated PDFs, including speed targets); `tst_search.qml` drives it through the window (shortcuts,
      counter,
      highlights as pixels, per-tab state).
- `tst_quit` starts the real application headless and ends it with SIGTERM, SIGINT and SIGHUP like a logout does, then
  starts it again and checks the session.
- `tst_realpdfs.qml` repeats the tab-switch scenario on your own PDFs and is skipped unless you name them:

  ```bash
  SHEAF_TEST_PDFS="/path/one.pdf:/path/two.pdf" ctest --test-dir build -R tst_qml --output-on-failure
  ```

`scripts/tidy.sh` runs clang-tidy on all sources.

## License

Copyright (C) 2026 Boas Meier. Sheaf is free software: you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
option) any later version. It is distributed in the hope that it will be useful, but without any warranty. See
[LICENSE](LICENSE) for the full text.

`qml/SheafPdfView.qml` is a modified copy of Qt's `PdfMultiPageView.qml`, which Qt offers under the GPL-3.0 among
other licenses; its original copyright and license lines are kept in the file.

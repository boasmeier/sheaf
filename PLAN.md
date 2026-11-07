# sheaf: Implementation plan

A minimal, fast PDF viewer for students, with tabs, collapsible tab groups, session restore and a link back-stack.

## 1. Scope

### Features (v0.1)

1. **Remember position** per PDF (page, scroll offset, zoom), even after the file is moved or renamed.
2. **Tabs and tab groups**, shown as a vertical sidebar tree.
3. **Collapsible groups.**
4. **Session restore**: on start, reopen exactly the groups and tabs that were open at the last close, including collapsed state and the active tab.
5. **Link navigation** with a multi-level back/forward stack per tab (Alt+← / Alt+→).
6. **Lightweight and fast**: lazy tabs, lazy page rendering, low idle CPU.

### Target

- Ubuntu with GNOME (Wayland), the developer's own machine.
- No packaging or distribution yet. Build and run from source.
- Cross-platform later (Windows, macOS), so avoid Linux-only APIs.

### Non-goals for v0.1

Annotations and editing, printing, forms, non-PDF formats, cloud features, plugin system, packaging (AppImage, Flatpak, installers).

## 2. Stack and dev setup

| Area | Choice |
|---|---|
| Language | C++20 |
| UI | Qt 6 (≥ 6.4), Qt Quick / QML |
| PDF | Qt PDF (`Qt6::Pdf`, `Qt6::PdfQuick`), PDFium-based |
| Persistence | SQLite via `Qt6::Sql` |
| Build | CMake ≥ 3.21, Ninja |
| Tests | Qt Test (`Qt6::Test`) |
| Licence | GPL-3.0-or-later (decide before first public push) |

### Setup

- Install Qt 6 from apt: the base, declarative (QML) and PDF development packages, plus CMake and Ninja.
- **Look up the exact package names on the machine first** (`apt search qt6 pdf`). Qt PDF ships with the Qt WebEngine packages and is easy to miss.
- Verify with a minimal CMake project that `find_package(Qt6 REQUIRED COMPONENTS Quick Pdf PdfQuick Sql Test)` succeeds before writing app code.
- No vcpkg for now. CMake's `find_package` stays the same if Qt later comes from vcpkg or the Qt installer.

## 3. Architecture

### Overview

```
QML UI
  Main.qml ─ Sidebar.qml (groups + tabs tree)
          └─ Viewer.qml  (PdfMultiPageView for the active tab)
              │
C++ backend (exposed to QML)
  SessionModel     tree model: groups → tabs (QAbstractItemModel)
  TabController    lazy loading/unloading of documents, active tab
  PositionStore    per-document position (page, offset, zoom)
  SessionStore     groups, tabs, order, collapsed state, active tab
  Database         SQLite connection, migrations
  FileIdentity     stable key per PDF file (survives rename/move)
```

### Key decisions

- **Viewer:** use the QML `PdfMultiPageView` from Qt PDF. It renders lazily, has a page cache, and provides back/forward navigation (`back()`, `forward()`, `backEnabled`, `forwardEnabled`). Only fall back to a custom view if performance or position restore turns out insufficient (see Risks).
- **One viewer instance**, re-bound to the active tab's document. Each tab owns its `PdfDocument` while loaded, plus its own navigation history.
- **Lazy tabs:** a tab stores only path, file key and title until first activated. Its document is created on activation.
- **Unloading:** documents of tabs not viewed for N minutes (default 10) are unloaded. Their position is saved first, so reloading is seamless.
- **Sidebar instead of a horizontal tab bar.** Groups are expandable rows, tabs are children. Every tab belongs to a group; new files go to the active group (or a default "Ungrouped" group).
- **File identity:** key = SHA-256 of (file size + first 64 KiB + last 64 KiB). Store the last known path too. On open, look up position by key, so moved or renamed files keep their position.

### Data model (SQLite)

Database at `~/.local/share/sheaf/sheaf.db` (use `QStandardPaths::AppDataLocation`, so other platforms work later).

```sql
CREATE TABLE schema_version (version INTEGER NOT NULL);

CREATE TABLE documents (
  file_key   TEXT PRIMARY KEY,
  last_path  TEXT NOT NULL,
  page       INTEGER NOT NULL DEFAULT 0,
  offset_y   REAL    NOT NULL DEFAULT 0,  -- position within the page, 0..1
  zoom       REAL    NOT NULL DEFAULT 1,
  updated_at INTEGER NOT NULL             -- unix time
);

CREATE TABLE groups (
  id        INTEGER PRIMARY KEY,
  name      TEXT    NOT NULL,
  position  INTEGER NOT NULL,
  collapsed INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE tabs (
  id       INTEGER PRIMARY KEY,
  group_id INTEGER NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
  path     TEXT    NOT NULL,
  file_key TEXT,
  position INTEGER NOT NULL
);

CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT);
-- e.g. active_tab_id, window geometry, sidebar width
```

### Persistence rules

- Save position **debounced** (500 ms after the last page/scroll/zoom change) and on tab switch, tab close and app quit.
- Save session changes (groups, tabs, order, collapsed state, active tab) immediately; they are infrequent.
- Use transactions, so a crash never leaves a half-written session.
- Navigation history (back/forward stacks) is **in memory only** in v0.1.

### Missing files on restore

A tab whose file no longer exists at its path is shown greyed out with a "File not found" state, offering "Locate…" (file dialog; re-link by file key if it matches) or "Remove". The app never fails to start because of a missing file.

## 4. Milestones

Build one milestone at a time. Each ends with a working app and a check by the developer with real course PDFs before the next one starts.

### M0: Skeleton

- CMake project, QML window, Database with migrations, empty test target.
- **Done when:** `cmake --build` succeeds, the app opens an empty window, `ctest` runs.

### M1: Viewer

- Open a PDF via file dialog or command-line argument. Continuous vertical scroll, zoom (Ctrl+wheel, Ctrl+±, fit-width), page number display and "go to page".
- **Done when:** a 300+ page script opens in under 1 s and scrolls smoothly; jumping to page 250 is instant; zoom re-renders sharply.

### M2: Lazy tabs and position memory

- Multiple PDFs as tabs (flat list for now). Lazy loading and unloading. PositionStore with FileIdentity.
- **Done when:** with 20 tabs open, only viewed tabs use memory; closing and reopening a PDF returns to the same page, offset and zoom; a renamed file keeps its position.

### M3: Links and back/forward

- Internal links jump within the document; external links open in the default browser (ask first). Alt+← / Alt+→ and mouse back/forward buttons. History per tab.
- **Done when:** following three links in a row and pressing back three times returns to the exact starting position; switching tabs keeps each tab's history.

### M4: Sidebar with groups

- Vertical tree: create, rename, delete groups; collapse/expand; drag-and-drop tabs within and between groups; reorder groups. Keyboard: Ctrl+Tab cycles tabs, Ctrl+W closes, Ctrl+O opens into the active group.
- **Done when:** all operations work with mouse and keyboard, and the tree stays consistent after any sequence of moves.

### M5: Session restore

- Persist and restore groups, tabs, order, collapsed state, active tab, window geometry. Missing-file handling.
- **Done when:** quitting (window close, Ctrl+Q, logout) and restarting restores the identical sidebar and active tab at the remembered position; a deleted file shows the "File not found" state.

### M6: Search

- Search bar per tab using Qt PDF's search model. Highlight all matches, jump between them, show a "3 of 17" counter. Search state (query, current match) is kept per tab.
- **Shortcuts:** Ctrl+F opens the bar and focuses it (selecting the existing query); Enter or F3 next match; Shift+Enter or Shift+F3 previous match; Esc closes the bar and clears highlights.
- **Done when:**
  - on a 300+ page script, the first match is highlighted within 200 ms, and the full match count is ready within 2 s;
  - results appear incrementally and the UI never blocks: scrolling stays smooth while a search runs;
  - typing is debounced (about 150 ms), so each keystroke does not start a full new search;
  - switching tabs keeps each tab's query and current match;
  - a scanned PDF without a text layer shows "No text in this document" instead of silently finding nothing.

### M7: Text selection and copy

- Select text with the mouse, copy to the clipboard. Right-click menu with "Copy".
- **Shortcuts:** mouse drag selects; double-click selects a word; Ctrl+C copies; Ctrl+A selects all text of the current page; Esc clears the selection.
- **Done when:**
  - the selection follows the mouse without visible lag;
  - copying a full page's text takes under 100 ms;
  - selection does not interfere with link clicks (a short click on a link still follows it);
  - behaviour when dragging across a page boundary is tested and documented (supported, or clearly limited).

### M8: Dark mode and inverted pages

- The UI follows GNOME's light/dark style. Separately, an option to invert page colours for reading at night.
- **Shortcuts:** Ctrl+Shift+I toggles inverted pages (global, remembered across restarts).
- **Done when:**
  - toggling takes effect within 100 ms on the visible pages;
  - inverting is done as a display effect (shader), not by re-rendering, so it costs no extra RAM and no re-render;
  - images in inverted mode are acceptable (test with a script containing photos and plots).

### M9: Outline panel

- Show the PDF's table of contents (bookmarks) as a collapsible tree, and highlight the current section while scrolling.
- **Shortcuts:** F7 toggles the outline panel; Enter on an entry jumps to it.
- **Done when:**
  - the outline of a 300+ page script with 500+ entries opens within 100 ms;
  - jumping via the outline pushes onto the back stack, so Alt+← returns to the previous position;
  - PDFs without an outline show a clear "No outline" state.

### M10: Settings dialog

- Settings for: tab unload timeout, default zoom (fit width / fit page / last used), page cache size, inverted pages default, restore session on start (on/off).
- **Shortcuts:** Ctrl+, opens the dialog; Esc closes it.
- **Done when:**
  - changes apply immediately without restarting and persist in the `settings` table;
  - the dialog opens within 100 ms;
  - invalid values are impossible (constrained inputs, sensible ranges).

### M11: Open files from the command line into a group

- `sheaf a.pdf b.pdf` opens files into the active group; `sheaf --group "Analysis" *.pdf` opens them into the named group, creating it if needed.
- Single instance: if sheaf is already running, files are handed to the running window (via `QLocalServer`/`QLocalSocket`) instead of starting a second instance.
- **Done when:**
  - opening 20 files at once shows the window within 500 ms, with only the first file loaded (others lazy);
  - handing files to a running instance takes under 200 ms and brings its window to the front;
  - files that are already open are focused, not duplicated;
  - non-existent or non-PDF paths produce a clear message and do not abort the other files.

## 5. Testing and performance

### Automated (Qt Test)

- FileIdentity: same key after rename/move; different key for different files.
- PositionStore and SessionStore: round-trips, debouncing, schema migration from empty DB.
- SessionModel: add/move/remove tabs and groups keeps positions consistent.

### Manual checklist per milestone

Test with real material: a long lecture script (300+ pages), a math-heavy PDF, a scanned PDF, and a PDF with many internal links (e.g. a LaTeX document with hyperref).

### Performance targets

| Metric | Target |
|---|---|
| Cold start with restored session (20 tabs, 1 active) | < 500 ms to first rendered page |
| RAM with 20 tabs, 2 loaded | < 250 MB |
| Idle CPU | ~0 % (no polling, no animations while idle) |
| Page jump (go to page) | visually instant (< 100 ms to low-res page) |

Measure with `btop` and `/usr/bin/time -v` and record results per milestone.

## 6. Repo layout and conventions

```
sheaf/
  CMakeLists.txt
  src/
    main.cpp
    app/        TabController, SessionModel
    storage/    Database, PositionStore, SessionStore, migrations
    util/       FileIdentity
  qml/
    Main.qml  Sidebar.qml  Viewer.qml  components/
  tests/
  README.md  LICENSE
```

- Qt naming conventions (CamelCase classes, camelCase methods).
- Business logic in C++, QML only for presentation.
- `clang-format` with a checked-in config; warnings as errors in debug builds.
- One commit per logical step, conventional commit messages.

### Working rules for the coding agent

1. Implement **one milestone at a time**, then stop and report: what was built, how to run it, and how each acceptance criterion was checked.
2. Before M0, look up the actual Qt 6 apt package names and the installed Qt version; stop if it is below 6.4.
3. Prefer Qt PDF's built-in components. Do not write a custom renderer or view unless asked.
4. Keep dependencies to Qt only.
5. Write tests for storage and models as part of each milestone, not afterwards.

## 7. Risks and open questions

- **Exact scroll-position restore:** `PdfMultiPageView` exposes the current page and navigation, but restoring a precise offset within a page may need access to its internal list view. Fallback: restore page + zoom only, or a custom view (M1/M2 decide).
- **Back-stack granularity:** check that Qt PDF's navigation history records the exact scroll position before a link jump, not just the page.
- **Drag-and-drop in a QML tree** is the most fiddly UI part (M4). Keep the model logic in C++ with tests.
- **Large scanned PDFs** may need a smaller page cache or lower render scale while scrolling.
- **Open questions:** licence (GPL-3.0 vs. other); should closing the last tab of a group delete the group; should groups be colour-coded.
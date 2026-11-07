# Sheaf

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/sheaf
./build/tests/tst_database
```

Open a PDF with Ctrl+O, or pass it on the command line: `./build/sheaf file.pdf`.

| Key | Action |
|---|---|
| Ctrl+O | Open |
| Ctrl+wheel, Ctrl++, Ctrl+- | Zoom |
| Ctrl+0 | Fit width |
| Ctrl+G | Go to page |
| Alt+Left / Alt+Right, mouse Back / Forward | Back / forward through followed links (per tab) |
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous tab (all groups, in sidebar order) |
| Ctrl+W | Close tab |
| Ctrl+O | Open files into the active group |
| Ctrl+Shift+N | New group (named right away) |
| F2 | Rename the active group |
| Ctrl+Shift+Up / Down | Move the active tab up / down, across groups |

## Sidebar

Tabs live in groups. Click a tab to show it; click a group to make it the target for new files; click the arrow to
collapse it; double-click a group to rename it; right-click for a menu (rename, move, delete, move a tab to another
group); drag tabs and groups to reorder them. With the sidebar focused: Up/Down move the cursor, Return/Space show a
tab or fold a group, Left/Right fold and unfold, F2 renames, Delete closes a tab or deletes a group. Deleting a group
closes its tabs (after asking); the position in each document is remembered.

## Tests

```
ctest --test-dir build --output-on-failure
```

- `tst_database`, `tst_fileidentity`, `tst_positionstore`, `tst_tabcontroller`: C++ unit tests.
- `tst_qml`: QML tests in `tests/qml/` against the real QML sources with generated fixture PDFs (headless, software
  rendering): viewer behaviour (`tst_viewer.qml`), position restore without wrong or half-rendered pages
  (`tst_restore.qml`), the real window with the real tab controller (`tst_tabswitch.qml`) and link navigation
  with back/forward history (`tst_navigation.qml`), and the sidebar and group shortcuts (`tst_sidebar.qml`,
  `tst_shortcuts.qml`).
- `tst_realpdfs.qml` repeats the tab-switch scenario on your own PDFs and is skipped unless you name them:

  ```
  SHEAF_TEST_PDFS="/path/one.pdf:/path/two.pdf" ctest --test-dir build -R tst_qml --output-on-failure
  ```

`scripts/tidy.sh` runs clang-tidy on all sources.

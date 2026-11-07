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
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous tab |
| Ctrl+W | Close tab |

## Tests

```
ctest --test-dir build --output-on-failure
```

- `tst_database`, `tst_fileidentity`, `tst_positionstore`, `tst_tabcontroller`: C++ unit tests.
- `tst_qml`: QML tests in `tests/qml/` against the real QML sources with generated fixture PDFs (headless, software
  rendering): viewer behaviour (`tst_viewer.qml`), position restore without wrong or half-rendered pages
  (`tst_restore.qml`), the real window with the real tab controller (`tst_tabswitch.qml`) and link navigation
  with back/forward history (`tst_navigation.qml`).
- `tst_realpdfs.qml` repeats the tab-switch scenario on your own PDFs and is skipped unless you name them:

  ```
  SHEAF_TEST_PDFS="/path/one.pdf:/path/two.pdf" ctest --test-dir build -R tst_qml --output-on-failure
  ```

`scripts/tidy.sh` runs clang-tidy on all sources.

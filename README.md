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
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous tab |
| Ctrl+W | Close tab |

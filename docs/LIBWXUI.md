# DevTools UI boundary

DevTools follows brostu's XML layout and named-control notification pattern.
The complete workbench is in `res/workbench.xml`, embedded at configure time.
`src/app.cpp` owns SDK jobs and application state; controls, layout, native
dialogs, window lifetime and UI-thread delivery belong to `3rdparty/libwxui`.

Application code must not include `wx/*`, use native `wx*` types/functions,
or call native methods through a libwxui control's manager. Add a reusable
libwxui API when an application feature needs a missing platform capability.
`devtools.ui_boundary` checks application sources for native includes/symbols.

- `DesktopWindow` composes the native frame, loads XML and resolves typed
  controls with `Require<T>()`. It provides status text, file/directory
  pickers, confirmation/error dialogs and a read-only text dialog.
- `Poster()` returns a copyable sender callable from a worker thread. Accepted
  callbacks execute on the UI thread. Closing or destroying the window cancels
  pending callbacks and makes retained senders return `false`. Window methods
  and controls themselves must only be used on the UI thread.
- `OnClose()` can defer an ordinary close while the SDK saves data. Stop and
  join producers before `FinishClose()` or before allowing a forced close.
- `Application::Arguments()` returns UTF-8 arguments for `OnAppInit()` to
  parse; libwxui does not reject application-specific command-line options.
  `WXUI_IMPLEMENT_APPLICATION` supplies platform entry points.
- `Edit` supports UTF-8 values and hints. `RichEdit::AppendBounded()` retains
  a bounded number of Unicode code points, including supplementary characters.

Run `cmake --build --preset windows-debug` and
`ctest --preset windows-debug`. The `libwxui.desktop` test covers XML controls,
Unicode, filtering/reselection, tree arrow keys, tab visibility, worker delivery and callback
cancellation. It requires a desktop session (or Xvfb on Linux); headless core
checks can use `ctest --test-dir <build> -LE gui --output-on-failure`.

For an end-to-end GUI check, launch `sovkit-devtools --smoke-report <path>`.
It loads the bundled SDK, runs `selftest`, and records the response round trip,
UI backend and editor dimensions without exporting request data. Close the
window normally after checking the report.

## Appearance

The desktop host registers the bundled Noto Sans CJK SC UI font and Noto Sans
Mono CJK SC editor font for the process, both at 11 pt. The same Chinese/Latin
glyphs ship on all platforms; font rasterization still belongs to the OS.
Fonts and their OFL license are staged into `fonts/` beside the executable
(`Contents/Resources/fonts/` for macOS). They are not installed system-wide.

Labels and buttons default to vertical centering. Multiline editors keep text
aligned to the top for reading and editing. RichEdit and JsonViewer share a
Scintilla adapter with custom scrollbars that appear only on overflow. Tree
lists, standalone scrollbars and splitters use the same light/dark palette.
JSON syntax colors adapt to the editor background. Native single-line and
password inputs retain platform IME and password behavior.

Splitter dragging lays out and invalidates only its own container, without
background erasure. Native editor geometry is applied only when it changes;
scrollbar updates retain the current viewport instead of resizing it repeatedly.
The desktop regression test observes native paint/erase/size events during
both workbench splitter drags and verifies stable scrolling causes no resize.

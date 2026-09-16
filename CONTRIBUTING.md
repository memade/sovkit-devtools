# Contributing / 参与贡献

Thank you for helping make SovKit easier to integrate and debug. English and Chinese issues and pull requests are welcome.

欢迎用中文或英文提交问题与 PR。当前最需要 Windows/Linux 实机验证、可复现的 SDK 接入问题，以及小范围的 UI、文档和构建修正。

## Before changing code

Read the [README](README.md) and [architecture notes](docs/ARCHITECTURE.md). DevTools must remain an independent SDK consumer: public header, dynamic library, and integration documentation only. Do not add dependencies on private SovKit or OrbitBridge sources.

Obtain a matching SDK package, then use the build script and `--test` described in the README. Platform presets are configuration entry points, not proof of validation. State which OS, architecture, SDK version/ABI, and build configuration you actually tested.

## Report a problem

Include the tool version/commit, SDK version/ABI, OS/architecture, steps, expected result, and actual result. For communication problems, include both peers' configurations and which step failed. Minimal synthetic examples are especially useful.

Use the GUI metadata export when helpful. Review anything else before attaching it: raw responses, logs, profile directories, identity exports, passwords, and message contents can contain private data. Do not upload real credentials or production profiles.

## Submit a change

- Keep a PR focused and explain the observable problem and resulting behavior.
- Keep SDK allocations paired with SDK frees and keep callbacks alive until the SDK has safely stopped and detached them.
- Add or adjust tests when fixing behavior that could regress. Run relevant existing tests; describe any platform checks you could not run.
- Update user-facing documentation when capabilities, limitations, or setup change.
- Keep binaries, build caches, local SDK packages, and test profiles out of Git.

Current priority: Windows/Linux build and runtime validation, Unicode paths, DPI/layout behavior, two-machine communication, and packaging. Broader networking and native GATT integration should be discussed in an issue before a large implementation.

Contributions to DevTools-owned code use the repository's MIT license. Preserve third-party notices and SDK licensing boundaries.

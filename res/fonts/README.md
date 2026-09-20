# Bundled typography

Unmodified Noto CJK fonts from https://github.com/notofonts/noto-cjk (Sans 2.004),
distributed under the accompanying SIL Open Font License in `OFL.txt`.

- UI: `Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf`
  SHA-256: `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`
- Editors: `Sans/Mono/NotoSansMonoCJKsc-Regular.otf`
  SHA-256: `ec04cc376b34887cedbdf84074e2e226ed2761eeabdcb9173fc1dd7bfd153ef7`

Both include Chinese and Latin glyphs, avoiding a separate OS-dependent Chinese
fallback in the JSON editor. libwxui registers these privately for this process;
it does not install fonts into the user's operating system. Ship this directory
beside the executable, or under `Contents/Resources` in a macOS app bundle.

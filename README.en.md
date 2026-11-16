# oronbox_wasm_modules

[简体中文](README.md) | English

WebAssembly modules for QuickJS JavaScript execution, Opus frame decoding, and WebP encoding in OronBox

## Integration

Download `oronbox_wasm_modules.zip` from a Release, verify its accompanying SHA-256, and bundle the required modules as application assets for the host Wasm runtime

| Module | Purpose | Wrapper source |
|---|---|---|
| `quickjs.wasm` | QuickJS JavaScript execution and host bridge | `quickjs/quickjs_bridge.c` |
| `opus_frame_decoder.wasm` | Opus audio frame decoding | `opus_decoder/opus_decoder.c` |
| `webp_encoder.wasm` | RGBA-to-WebP encoding | `webp_encoder/webp_encoder.c` |

The ZIP includes all three modules, a build manifest, and licenses. The `manifest.json` records the source commit, workspace state, toolchain versions, upstream sources, and module hashes

## Local development

Install Zig, CMake, and Python. Binaryen’s `wasm-opt` optimizes the output. Toolchain versions used by CI are specified in the workflow

Build and package all modules

~~~sh
bash tool/build.sh
~~~

Build an individual module

~~~sh
sh quickjs/build.sh
sh opus_decoder/build.sh
sh webp_encoder/build.sh
~~~

Output goes to `dist` by default. Set `OUTPUT_DIR` to choose another directory

Build scripts download and verify upstream sources in `.build`. Source URLs, versions, and SHA-256 hashes are pinned in `tool/sources.json` and updated together

## License

Repository-owned wrappers and scripts use [Apache-2.0](LICENSE). Third-party copyrights and licenses are listed in [THIRD_PARTY_NOTICES.en.md](THIRD_PARTY_NOTICES.en.md)

# oronbox_wasm_modules

简体中文 | [English](README.en.md)

为 OronBox 提供 QuickJS JavaScript 执行、Opus 帧解码和 WebP 编码所需的 WebAssembly 模块

## 接入

下载 Release 中的 `oronbox_wasm_modules.zip`，校验配套 SHA-256 后，将所需模块作为应用资源分发，通过宿主的 Wasm 运行时加载

| 模块 | 用途 | 封装源码 |
|---|---|---|
| `quickjs.wasm` | QuickJS JavaScript 执行与宿主桥接 | `quickjs/quickjs_bridge.c` |
| `opus_frame_decoder.wasm` | Opus 音频帧解码 | `opus_decoder/opus_decoder.c` |
| `webp_encoder.wasm` | RGBA 图片编码为 WebP | `webp_encoder/webp_encoder.c` |

发布附件为 `oronbox_wasm_modules.zip` 和配套 SHA-256 文件，ZIP 包含三个 Wasm 模块、构建清单和许可证

构建清单 `manifest.json` 记录源码提交、工作区状态、工具链版本、上游来源与模块哈希

## 本地开发

安装 Zig、CMake 和 Python，使用 Binaryen 的 `wasm-opt` 优化产物，CI 使用的工具链版本见工作流配置

构建并打包全部模块

```sh
bash tool/build.sh
```

也可分别构建单个模块

```sh
sh quickjs/build.sh
sh opus_decoder/build.sh
sh webp_encoder/build.sh
```

产物默认写入 `dist`，可通过 `OUTPUT_DIR` 指定输出目录

上游源码由构建脚本下载到 `.build` 并校验，来源、版本与 SHA-256 固定在 `tool/sources.json`，升级时同步更新这三项

## 许可证

本仓库的封装代码与构建脚本采用 [Apache-2.0](LICENSE)，第三方组件的版权与许可证见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

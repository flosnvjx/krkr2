# KrKr2 UI 与外壳层

> **状态：** 设计草案（待评审）  
> **原则：** JS 管外壳 · C++ 管引擎 · 稳定 C ABI 连接两端 · 渐进替换 Cocos2d-x

---

## 背景

KrKr2 当前使用 **Cocos2d-x 3.17.2** 同时承担：

1. **外壳 UI** — 文件选择、设置、游戏内菜单、消息框等（`cpp/core/environ/ui/`）
2. **游戏视口** — KAG 画面合成、LayerBitmap、YUV 视频（`MainScene`、`YUVSprite` 等）

Cocos2d-x 已长期停更，UI 表现力不足。新方案采用：

| 平台 | 外壳 UI | 游戏渲染 |
|------|---------|----------|
| Windows / Linux / macOS | **Electron + React** | Native GL（逐步脱离 Cocos） |
| Android（及未来 iOS） | **React Native** | Native GL（`KrkrGameView`） |

业务逻辑与类型定义在 **`packages/shared`** 跨端复用；引擎实现保持在 **`cpp/`**，通过 **`bridge/`** 暴露稳定 C ABI。

---

## 阅读顺序

| 顺序 | 文档 | 内容 |
|:----:|------|------|
| 1 | [architecture.md](architecture.md) | 分层、进程模型、平台对照 |
| 2 | [monorepo.md](monorepo.md) | 仓库目录、包划分、构建 |
| 3 | [bridge.md](bridge.md) | `krkr_engine_*` API、JS Bridge、线程约定 |
| 4 | [migration.md](migration.md) | 分阶段迁移与回滚策略 |
| 5 | [rendering.md](rendering.md) | `krkr2-render` 与 Cocos 解耦 |

---

## 设计目标

| 目标 | 做法 |
|------|------|
| **现代 UI** | React 生态 + 成熟组件库 |
| **跨端一致** | 共享 `packages/shared`（状态、类型、i18n、Bridge 契约） |
| **语言边界清晰** | UI 在 `apps/` + `packages/`；引擎在 `cpp/`；契约在 `bridge/` |
| **渐进迁移** | 先换外壳 UI，再抽渲染，最后移除 `cocos2dx` 依赖 |
| **与 Rust 层共存** | 插件迁移走 `docs/rust/` 既有路径，UI 层不重复造 FFI |

---

## 不在范围内

- 重写 KAG / TJS 脚本引擎
- 用 WebGL 在浏览器内渲染游戏画面
- 以 Tauri / 纯 WebView 作为本文档的主方案（Electron + RN 为已定方向）

---

## 相关链接

| 资源 | 说明 |
|------|------|
| [RUST_LAYER.md](../RUST_LAYER.md) | Rust 插件迁移 |
| `cpp/core/environ/ui/` | 现有 Cocos UI 实现（迁移源） |
| `cpp/core/environ/cocos2d/` | 现有 Cocos 集成（渲染迁移源） |
| `platforms/android/app/` | 现有 Android 入口 |

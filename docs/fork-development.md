# 产品 Fork 开发与上游同步

## 仓库与基线

- 产品仓库（`origin`）：https://github.com/Inupedia/snow-apps
- 原始仓库（`upstream`）：https://github.com/mg-chao/snow-apps
- 产品开发分支：`codex/product-development`
- 产品稳定分支：`main`
- 初始化日期：2026-10-26。
- 已在本机完成 macOS ARM64 Debug 构建、部署和启动的上游基线：
  `37a9108c5a0cb9e07f0f84148f641438c2c6aeda`。

Fork 初始化时，上述基线与 `origin/main`、`upstream/main` 一致。
保留完整 Git 历史；不通过复制源码、删除 `.git` 或重新初始化仓库建立产品。
本地默认推送远程和 GitHub CLI 默认仓库已设为产品仓库。
这些 Git 配置只作用于当前 checkout，新克隆后需要重新设置。

## 日常开发

在 `codex/product-development` 上集成产品改造。独立功能使用
`codex/<feature>` 分支，合入产品开发分支；准备发布时再将经过验证的
产品开发分支合入自己的 `main`。产品发布标签使用 `product-v*`，
避免与上游标签混淆。不要向 `upstream` 推送产品改造。

当前 Mac 使用 `$HOME/snow-apps-dev` 作为原仓库的软链接，避免
`Side Projects` 中的空格破坏第三方构建脚本。依赖和构建缓存不提交到 Git。

```sh
cd "$HOME/snow-apps-dev"
export NINJA=/opt/homebrew/opt/ninja/bin/ninja
export CARGO_BUILD_JOBS=4
scripts/run-snow-shot.sh snow-shot-macos-arm64-debug
```

只启动已部署的开发版：

```sh
scripts/run-snow-shot.sh snow-shot-macos-arm64-debug --no-build
```

上述本机命令依赖已安装的 Qt 和构建工具。新机器应按
[`docs-macos-build.md`](../docs-macos-build.md) 完成 bootstrap，不能依赖
当前机器的缓存或软链接。

## 大幅修改界面时的边界

当前界面使用 C++ Qt Widgets，没有前端 HMR。选择继续使用 Widgets，
或者引入 QML / Web 界面，应在界面迁移前确定；本次 Fork 初始化没有改变技术栈。

建议后续的新界面集中放在 `snow_shot/src/presentation/product/`，
对应头文件放在 `snow_shot/include/snow_shot/presentation/product/`。
这些目录是约定，尚未创建。应用入口和现有主窗口只保留必要的接入改动。
用适配层调用现有截图、录屏、OCR、历史记录和设置服务。
适配层负责转换产品界面的数据和命令，不复制底层实现。

`presentation/` 中也包含 `services/`、截图控制器和与原生窗口相关的交互代码，
不能将整个目录当作可以直接替换的纯皮肤层。先梳理依赖，再逐步替换页面。

尽量保留 `snow-crates/`、`snow_rust_ffi/`、`snow_image/` 和
`snow_draw_engine_qt/` 的上游实现与路径。底层修复、界面重构、品牌替换
分别提交，方便合并时判断冲突的业务含义。避免全仓库批量改名或格式化。
不要配置 `merge=ours` 或使用全局 `-X ours` 自动忽略上游改动。

新上下文和文案继续遵守根目录 `AGENTS.md` 的翻译规则。
界面参数热重载或自动增量构建可以作为独立开发工具加入；修改 C++ 逻辑
仍需编译。日常手动看界面不必每次执行测试，行为变更完成后只运行相关测试。

## 品牌与发布配置

产品名称尚未确定，本次没有修改应用身份或发布配置。实现产品品牌时，
应先增加集中配置，再逐一替换接入点，至少检查：

- 名称、产品 ID、可执行文件名、设置存储标识、MCP 名称：
  `snow_shot/include/snow_shot/app/edition.h`。
- 图标、资源、关于页、隐私说明和三种语言的文案。
- macOS bundle ID、签名标识、权限提示和安装器中的应用识别规则：
  `snow_shot/CMakeLists.txt`、`cmake/SnowShotMacOS.cmake` 及打包脚本。
- Windows 安装器、应用 ID、协议和注册表标识（如果支持 Windows）。
- API、官网、项目链接、版本查询、安装包与更新下载地址。
  当前 `snow_shot/src/update/macosupdateservice.cpp` 仍包含原项目的
  GitHub/Gitee 发布地址；仅修改 Git remote 不会改变应用内更新来源。
- `.github/workflows/`、Homebrew/Scoop/WinGet 发布和发布脚本中的仓库与渠道。

发布自己的产品前，更新机制必须指向自己的发布渠道，或者明确禁用。
不要让产品用户通过应用内更新被替换回原版 Snow Shot。
修改应用身份后，应预期 macOS 需要重新授权；设置和历史数据是否迁移应显式设计。

## 合并上游更新

优先选择已发布、经过验证的上游版本；需要最新修复时才直接同步
`upstream/main`。同步应集中进行，不在每个界面提交中混入上游更新。
下面以同步 `upstream/main` 为例。

先提交或另行保存当前改动，确认 `git status --short` 为空，然后执行：

```sh
git fetch origin
git fetch upstream
git switch codex/product-development
git pull --ff-only
git log --oneline HEAD..upstream/main
git diff --stat HEAD...upstream/main
git switch -c codex/sync-upstream-YYYYMMDD
git merge --no-ff upstream/main
```

将 `YYYYMMDD` 换成本次同步日期，必要时追加序号，确保分支名唯一。
`git pull --ff-only` 如果失败，先检查分歧，不使用强制重置覆盖产品提交。
如果选择同步发布标签，先显式获取该标签，再将合并目标替换为该标签。

出现冲突后逐个确认语义：保留产品界面的设计，同时接入上游服务接口变化、
修复和资源要求。不要整批选择 ours/theirs。解决完成后用
`git add <已解决的文件>` 和 `git merge --continue` 完成合并。
决定放弃本次合并时使用 `git merge --abort`，回到同步前状态。

合并成功后执行：

```sh
git submodule update --init --recursive
git diff --check
scripts/build.sh snow-shot-macos-arm64-debug --target snow_shot --parallel 4
```

构建前保留日常开发所需的工具环境变量。按实际变动选择相关测试，并手动
验证受影响的截图、标注、OCR 或录屏功能。禁止默认运行全量测试。
当上游修改工具链、Qt、vcpkg、OCR manifest 或打包逻辑时，先阅读其文档，
按需重新 bootstrap，再验证部署；不要只看是否成功编译。

`.gitmodules` 中 OCR 子模块继续使用原地址和父仓库锁定的提交。
不要使用 `git submodule update --remote` 擅自跳到子模块最新版本。
如果以后要修改 OCR 子模块，应单独 Fork 该仓库，再有意更新 `.gitmodules`
和 gitlink。

验证通过后，将同步分支推送到自己的 Fork：

```sh
git push -u origin codex/sync-upstream-YYYYMMDD
```

在 `Inupedia/snow-apps` 内创建 PR，以 `codex/product-development` 为 base。
PR 记录合入的上游提交、冲突处理和相关验证。用普通 merge 保留同步历史；
不要 squash 掉这类同步合并的祖先关系。共享的产品开发分支不反复 rebase
上游，也不 force-push。合入后继续从产品开发分支开发。

GitHub 的强制同步、`git reset --hard upstream/main` 或删除产品分支
都不是此产品的升级流程。

## 许可证

仓库保留原有多许可证结构，见 [`LICENSE.md`](../LICENSE.md)。
Snow Shot 主体及多个底层模块为 GPL-3.0-or-later，部分独立组件为 Apache-2.0。
产品改名不会改变已有代码的许可证。对外分发修改版时，应按适用许可证
保留通知、标明修改并向接收者提供相应源码；可以商业销售，但不能仅因
更换界面或品牌便将 GPL 覆盖的修改版作为闭源产品分发。
参考 [GNU GPLv3](https://www.gnu.org/licenses/gpl.en.html) 和
[GNU 官方 FAQ](https://www.gnu.org/licenses/gpl-faq.en.html)。

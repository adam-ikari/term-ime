# Website

This website is built using [Docusaurus](https://docusaurus.io/), a modern static website generator.

## Installation

```bash
npm install
```

**Note**: feel free to use the package manager of your choice.

## Local Development

```bash
npm run start
```

This command starts a local development server and opens up a browser window. Most changes are reflected live without having to restart the server.

## Build

```bash
npm run build
```

This command generates static content into the `build` directory and can be served using any static contents hosting service.

## 字体

站点用 **Sarasa Term SC Nerd**（Iosevka 派生，OFL-1.1，见
`src/assets/fonts/OFL.txt`）。选它的原因：笔画以直角为主 + 像素网格质感 +
边缘少量圆角，是终端字体的设计语言；自带 CJK，汉字宽恰好为拉丁 2 倍，
与终端对齐契约一致；Nerd Font 补丁提供 powerline / devicon 私用区字形。

上游是 28 MB 级 TTF，不可能直发。仓库只存**子集化**后的 woff2（三个字重各约
100 KB）与字符集清单 `src/assets/fonts/subsetchars.txt`。子集参数与覆盖率校验
都封装在 `scripts/font-subset.mjs` 里。

```bash
# 校验清单是否覆盖站点实际渲染文本（改完文案后跑）
npm run build
node scripts/font-subset.mjs check

# 有缺字就补进 subsetchars.txt，或直接重新生成清单 + woff2（需上游 TTF）
node scripts/font-subset.mjs write --src /path/to/sarasa-term-sc-nerd-fonts/
```

`write` 需要 `pip install fonttools brotli` 和上游 TTF
（`sarasa-term-sc-{regular,semibold,bold}-nerd-font.ttf`）。子集参数经字节级
验证，可用同一份清单复现仓库中的 woff2。

> 字体缺字会静默落到 fallback 字体，破坏全站字形统一。`check` 比对的「站点渲染
> 文本」= 构建产物 HTML 渲染文本 ∪ 首页 tsx 里的动态字符串（演示动画候选词在
> JS 运行后才出现，SSR HTML 抓不到）。不扫打包后的 js：那里混了 Prism 语言表
> （含西里尔、CJK 别名）等从不渲染的运行时数据，会误收噪声。

字体经 `src/css/custom.css` 的 `@font-face` 接入，三个字重映射到
400 / 600 / 700-800。`og-image.html` 复用同一份文件（生成分享卡片图见下节）。

## 分享卡片图（og-image.png）

`static/img/og-image.png`（1200×630，og/twitter 卡片图）由仓库根目录的 `website/og-image.html`
渲染而来。改文案后重新生成：

```bash
# 视口工具栏在 headless 下占 87px，故窗口取 717 高，再裁到 630
chromium --headless=new --no-sandbox --disable-gpu --disable-dev-shm-usage \
  --hide-scrollbars --window-size=1200,717 --virtual-time-budget=6000 \
  --screenshot=/tmp/og_raw.png "$PWD/og-image.html"
convert /tmp/og_raw.png -crop 1200x630+0+0 +repage static/img/og-image.png
```

## Deployment

Using SSH:

```bash
USE_SSH=true npm run deploy
```

Not using SSH:

```bash
GIT_USER=<Your GitHub username> npm run deploy
```

If you are using GitHub Pages for hosting, this command is a convenient way to build the website and push to the `gh-pages` branch.

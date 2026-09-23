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

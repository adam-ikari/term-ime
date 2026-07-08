import Terminal from './Terminal';

const CONFIG_JSON = `{
  "languages": [
    {"id": "zh-Hans", "name": "简体中文", "enabled": true},
    {"id": "zh-Hant", "name": "繁體中文", "enabled": true},
    {"id": "ja", "name": "日本語", "enabled": false}
  ],
  "active_language": "zh-Hans",
  "ui_language": "zh-CN",
  "ai_ranking": {
    "enabled": false
  }
}`;

export default function QuickStart() {
  return (
    <section id="quickstart" className="section quickstart">
      <div className="container">
        <h2>快速开始</h2>

        <h3>1. 安装系统依赖</h3>
        <Terminal title="bash">
          <span className="comment"># Debian / Ubuntu</span>
          {'\n'}
          <span className="prompt">$ </span>
          sudo apt-get install libuv1-dev libcurl4-openssl-dev rime-data-luna-pinyin
        </Terminal>

        <h3>2. 克隆并构建</h3>
        <Terminal title="bash">
          <span className="prompt">$ </span>
          git clone --recursive https://github.com/adam-ikari/term-ime.git
          {'\n'}
          <span className="prompt">$ </span>
          cd term-ime
          {'\n'}
          <span className="prompt">$ </span>
          make build
          {'\n'}
          <span className="prompt">$ </span>
          ./build/term-ime
        </Terminal>

        <p className="note font-tip">
          <span className="tip-icon">▶</span> 推荐在终端使用等宽字体(如{' '}
          <a href="https://github.com/subframe7536/maple-font" target="_blank" rel="noopener noreferrer">
            Maple Mono
          </a>
          、Sarasa Mono、JetBrains Mono),以获得最佳的中文与候选词对齐效果。
        </p>

        <h3>3. 配置(可选)</h3>
        <p className="note">
          配置文件位于 <code>~/.config/term-ime/config.json</code>。可在这里开关语言、
          切换界面语言、启用 AI 候选词排序。
        </p>
        <details>
          <summary>查看 config.json 示例</summary>
          <div style={{ marginTop: 12 }}>
            <Terminal title="~/.config/term-ime/config.json">
              <code className="json">{CONFIG_JSON}</code>
            </Terminal>
          </div>
        </details>

        <p className="note warn">⚠ 需在真实 TTY 或支持 alternate screen 的终端中运行。</p>
      </div>
    </section>
  );
}

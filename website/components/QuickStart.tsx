import Terminal from './Terminal';

const CONFIG_JSON = `{
  "languages": [
    {"id": "zh-Hans", "name": "简体中文", "enabled": true}
  ],
  "active_language": "zh-Hans",
  "ui_language": "zh-CN"
}`;

export default function QuickStart() {
  return (
    <section id="quickstart" className="section quickstart">
      <div className="container">
        <h2>快速开始</h2>

        <h3>用户安装（推荐，无需 sudo）</h3>
        <div className="quickstart-primary">
          <Terminal title="bash">
            <span className="comment"># 一键安装到 ~/.local/bin，完全静态，零依赖</span>
            {'\n'}
            <span className="prompt">$ </span>
            curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash
          </Terminal>
        </div>

        <h3>系统安装（需要 sudo）</h3>
        <div className="quickstart-primary">
          <Terminal title="bash">
            <span className="comment"># 安装到 /usr/local/bin，所有用户可用</span>
            {'\n'}
            <span className="prompt">$ </span>
            curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash -s -- --prefix /usr/local
          </Terminal>
        </div>

        <h3>从源码构建</h3>
        <div className="quickstart-src">
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
        </div>

        <p className="note font-tip">
          <span className="tip-icon">▶</span> 推荐在终端使用等宽字体(如{' '}
          <a href="https://github.com/subframe7536/maple-font" target="_blank" rel="noopener noreferrer">
            Maple Mono
          </a>
          、Sarasa Mono、JetBrains Mono),以获得最佳的中文与候选词对齐效果。
        </p>

        <h3>配置(可选)</h3>
        <p className="note">
          配置文件位于 <code>~/.config/term-ime/config.json</code>。可在这里开关语言、
          切换界面语言。
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

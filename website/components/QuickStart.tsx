import Terminal from './Terminal';

const CONFIG_JSON = `{
  "languages": [
    {"id":"zh-Hans","name":"简体中文","schema":"luna_pinyin_simp","enabled":true},
    {"id":"zh-Hant","name":"繁體中文","schema":"luna_pinyin","enabled":true},
    {"id":"zh-Hant-TW","name":"正體中文","schema":"terra_pinyin","enabled":false},
    {"id":"ja","name":"日本語","schema":"kana","enabled":false}
  ],
  "active_language": "zh-Hans",
  "ui_language": "zh-CN",
  "llama_ranker": {
    "enabled": false,
    "model_path": "",
    "n_threads": 2,
    "max_tokens": 10,
    "timeout_ms": 100,
    "backend": "cpu"
  },
  "log_level": "warn"
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
          sudo apt-get install librime-dev libuv1-dev rime-data-luna-pinyin
        </Terminal>

        <h3>2. 克隆并构建</h3>
        <Terminal title="bash">
          <span className="prompt">$ </span>
          git clone --recursive https://github.com/adam/term-ime.git
          {'\n'}
          <span className="prompt">$ </span>
          cd term-ime
          {'\n'}
          <span className="prompt">$ </span>
          make build{'  '}
          <span className="comment"># 或: cmake -B build && cmake --build build</span>
          {'\n'}
          <span className="prompt">$ </span>
          ./build/term-ime
        </Terminal>

        <h3>3. 配置(可选)</h3>
        <p className="note">
          配置文件位于 <code>~/.config/term-ime/config.json</code>(或{' '}
          <code>$XDG_CONFIG_HOME</code>)。命令行可传路径:{' '}
          <code>./build/term-ime /path/to/config.json</code>。
        </p>
        <details>
          <summary>查看默认 config.json</summary>
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

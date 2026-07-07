import Terminal from './Terminal';

export default function Hero() {
  return (
    <section id="hero" className="hero">
      <div className="container">
        <Terminal title="term-ime — zsh" className="hero-terminal">
          <span className="logo">
{`
  ▄█████▄   ▄█    ▄ ▄      ▄█    ▀▀    ▄█
  ██    ██  ██     ███    ██  ▄█▀    ██
  ██    ██  ██  ▄  ███▄   ██▄█▀ ▄█▀ ██
  ▀███████  ██  ▀ ████▀  ▀██ ▀▀ ▀▀▄▄██
`}
          </span>
          <span className="prompt">$ </span>
          <span className="cmd">term-ime</span>
          <span className="cursor" />
        </Terminal>

        <div className="lead">
          <h1>term-ime</h1>
          <p className="tagline">在 Linux TTY 中运行的虚拟终端,内置多语言输入法。</p>
          <p className="sub">
            基于 <span className="hl">librime</span> 的中文/日文输入,
            <span className="hl">libuv</span> 异步事件循环,
            可选 <span className="hl">llama.cpp</span> LLM 候选词排序,
            <span className="hl">FTXUI</span> 渲染。
          </p>
          <div className="cta">
            <a className="btn primary" href="#quickstart">快速开始</a>
            <a className="btn" href="#demo">交互演示</a>
          </div>
        </div>
      </div>
    </section>
  );
}

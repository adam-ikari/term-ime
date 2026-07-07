import TerminalDemo from './TerminalDemo';

export default function Hero() {
  return (
    <section id="hero" className="hero">
      <div className="container">
        <div className="hero-grid">
          <div className="lead">
            <h1>term-ime</h1>
            <p className="tagline">在 Linux TTY 中运行的虚拟终端,内置多语言输入法。</p>
            <p className="sub">
              中文、日文等多种语言输入,<span className="hl">候选词智能排序</span>,
              <span className="hl">终端 UI</span> 渲染,流畅的 TTY 打字体验。
            </p>
            <div className="cta">
              <a className="btn primary" href="#quickstart">快速开始</a>
              <a className="btn" href="#shortcuts">快捷键</a>
              <a
                className="btn"
                href="https://github.com/adam-ikari/term-ime"
                target="_blank"
                rel="noopener noreferrer"
              >
                GitHub
              </a>
            </div>
          </div>

          <div className="hero-demo">
            <div className="demo-chrome">
              <span className="dots" aria-hidden="true">
                <span className="dot dot-red" />
                <span className="dot dot-yellow" />
                <span className="dot dot-green" />
              </span>
              <span className="demo-title">term-ime — zsh</span>
            </div>
            <TerminalDemo className="demo-inner" />
          </div>
        </div>
      </div>
    </section>
  );
}

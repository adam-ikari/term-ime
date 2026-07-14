import { IconGitHub } from './icons';

export default function Footer() {
  return (
    <footer className="footer">
      <div className="container">
        <div className="line">
          <span className="prompt">$ </span>
          <span className="exit">exit</span>
        </div>
        <div className="meta">
          <a
            href="https://github.com/adam-ikari/term-ime"
            target="_blank"
            rel="noopener noreferrer"
          >
            <IconGitHub size={14} /> github.com/adam-ikari/term-ime
          </a>
          <span className="sep">·</span>
          <span>MIT License</span>
          <span className="sep">·</span>
          <span>Linux TTY 虚拟终端</span>
        </div>
      </div>
    </footer>
  );
}

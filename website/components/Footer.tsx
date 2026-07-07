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
            href="https://github.com/adam/term-ime"
            target="_blank"
            rel="noopener noreferrer"
          >
            <IconGitHub size={14} /> github.com/adam/term-ime
          </a>
          <span className="sep">·</span>
          <span>MIT License</span>
          <span className="sep">·</span>
          <span>C++17 / CMake / librime / llama.cpp / FTXUI</span>
        </div>
      </div>
    </footer>
  );
}

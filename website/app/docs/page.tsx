import type { Metadata } from 'next';

export const metadata: Metadata = {
  title: '文档 — term-ime',
  description: 'term-ime 配置、快捷键与使用说明',
};

const SECTIONS = [
  {
    id: 'shortcuts',
    title: '快捷键',
    body: `Ctrl+A 是前缀键,按下后下一个按键决定动作:

  Ctrl+A  Space    切换中英文模式
  Ctrl+A  A        开关 AI 候选词排序
  Ctrl+A  S        打开设置面板
  Ctrl+A  Ctrl+A   向 shell 发送字面 Ctrl+A
  Ctrl+A  其他键   向 shell 转发 Ctrl+A + 该键

输入候选状态:
  1-9       选择对应候选词
  Space     选择首个候选词
  Esc       取消输入
  Backspace 取消输入

设置面板:
  ↑/↓ 或 j/k   上下导航
  ←/→ 或 Enter 切换选项值
  Tab / Esc    关闭面板`,
  },
  {
    id: 'config',
    title: '配置',
    body: `配置文件位于 ~/.config/term-ime/config.json,可在这里:

  - 开关语言(简体中文 / 繁体中文 等)
  - 设置启动时激活的语言
  - 切换界面语言(中文 / 英文)
  - 启用 AI 候选词排序

首次运行会生成默认配置。命令行也可传路径覆盖:
  ./build/term-ime /path/to/config.json`,
  },
  {
    id: 'run',
    title: '运行',
    body: `构建后直接运行:
  ./build/term-ime

需在真实 TTY 或支持 alternate screen 的终端中运行(Ctrl+Alt+F1 切到 TTY,
或在 xterm / GNOME Terminal 等终端里)。

启动后默认英文模式,按 Ctrl+A Space 切换到中文,输入拼音即可看到候选词。
退出:在 shell 里输入 exit,或按 Ctrl+C。`,
  },
  {
    id: 'ai',
    title: 'AI 候选词排序',
    body: `term-ime 可选启用 AI 候选词排序,根据上下文把最可能的候选词排到最前。

启用:按 Ctrl+A A 切换。首次启用会下载一个小型语言模型(约 300MB),
之后即可使用。模型在后台加载,不影响打字。

关闭:再次按 Ctrl+A A。`,
  },
];

export default function DocsPage() {
  return (
    <div className="container docs-page">
      <nav className="docs-toc">
        <h3>文档</h3>
        <ul>
          {SECTIONS.map((s) => (
            <li key={s.id}>
              <a href={`#${s.id}`}>{s.title}</a>
            </li>
          ))}
        </ul>
        <p>
          <a href="/">← 返回首页</a>
        </p>
      </nav>

      <article className="docs-content">
        {SECTIONS.map((s) => (
          <section key={s.id} id={s.id} className="doc-section">
            <h2>{s.title}</h2>
            <pre>{s.body}</pre>
          </section>
        ))}
      </article>

      <style>{`
        .docs-page { display: grid; grid-template-columns: 200px 1fr; gap: 32px; padding: 32px 20px; }
        @media (max-width: 720px) { .docs-page { grid-template-columns: 1fr; } }
        .docs-toc h3 { color: var(--accent); font-size: 16px; margin-bottom: 12px; }
        .docs-toc h3::before { content: '# '; color: var(--text-faint); }
        .docs-toc ul { list-style: none; }
        .docs-toc li { margin: 6px 0; }
        .docs-toc a { color: var(--text-dim); font-size: 13px; }
        .docs-toc a:hover { color: var(--accent); }
        .docs-toc p { margin-top: 16px; }
        .doc-section { margin-bottom: 40px; }
        .doc-section h2 { color: var(--accent); font-size: 18px; margin-bottom: 12px; }
        .doc-section h2::before { content: '# '; color: var(--text-faint); }
        .doc-section pre {
          background: var(--bg-panel);
          border: 1px solid var(--border);
          border-radius: var(--radius);
          padding: 14px 16px;
          color: var(--text);
          font-size: 13px;
          line-height: 1.6;
          white-space: pre-wrap;
          word-break: break-word;
          overflow-x: auto;
        }
      `}</style>
    </div>
  );
}

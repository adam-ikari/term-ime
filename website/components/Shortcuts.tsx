import Terminal from './Terminal';

const ROWS: { keys: string[]; desc: string }[] = [
  { keys: ['Ctrl+A', 'Space'], desc: '切换中英文模式' },
  { keys: ['1', '-', '9'], desc: '选择候选词' },
  { keys: ['Space'], desc: '选择首个候选词(候选状态时)' },
  { keys: ['Esc'], desc: '取消输入 / 关闭设置面板' },
  { keys: ['Ctrl+A', 'A'], desc: '切换 AI 候选词排序' },
  { keys: ['Ctrl+A', 'S'], desc: '打开设置面板' },
  { keys: ['exit'], desc: '退出 shell' },
];

export default function Shortcuts() {
  return (
    <section id="shortcuts" className="section">
      <div className="container">
        <h2>快捷键</h2>
        <Terminal title="shortcuts">
          <table className="kbd-table">
            <tbody>
              {ROWS.map((r, i) => (
                <tr key={i}>
                  <td className="k">
                    {r.keys.map((k, j) => (
                      <span key={j}>
                        {j > 0 && <span className="plus"> </span>}
                        <kbd className="kbd">{k}</kbd>
                      </span>
                    ))}
                  </td>
                  <td className="d">{r.desc}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </Terminal>
      </div>
    </section>
  );
}

import Terminal from './Terminal';

const FEATURES = [
  '拼音输入 — 中文拼音输入法,候选词智能排序',
  '可扩展语言 — 配置驱动,自由增减语言,无需改代码',
  '智能候选词 — 可选 AI 候选词排序,贴合上下文',
  '完整中文支持 — UTF-8 编解码,CJK 宽字符对齐',
  '终端 UI — 候选栏、状态栏、设置面板,函数式渲染',
];

export default function Features() {
  return (
    <section id="features" className="section features">
      <div className="container">
        <h2>特性</h2>
        <Terminal title="features.txt">
          {FEATURES.map((f, i) => (
            <div key={i} className="line">
              <span className="bullet">●</span> {f}
            </div>
          ))}
        </Terminal>
      </div>
    </section>
  );
}

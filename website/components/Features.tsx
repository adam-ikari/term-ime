import Terminal from './Terminal';

const FEATURES = [
  '完整终端模拟 — 支持 VT100 转义序列、光标控制、滚动',
  '多语言输入 — 中文(拼音/双拼/注音)、日文等多种输入法',
  '可扩展语言 — 配置驱动,自由增减语言,无需改代码',
  '智能候选词 — 可选 AI 候选词排序,贴合上下文',
  '完整中文支持 — UTF-8 编解码,CJK 宽字符对齐',
  '终端 UI — 候选栏、状态栏、设置面板,函数式渲染',
  '快速响应 — 异步事件驱动,打字不卡顿',
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

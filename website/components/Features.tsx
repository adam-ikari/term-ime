import Terminal from './Terminal';

const FEATURES = [
  'PTY 虚拟终端 — 完整终端模拟,VT100 转义序列',
  '多语言输入法 — librime 引擎,中文 / 日文',
  '可扩展架构 — 语言配置化,避免硬编码',
  '异步事件驱动 — libuv 高性能事件循环',
  '智能候选词 — 可选 llama.cpp LLM 排序',
  'UTF-8 + CJK — 完整编解码,宽字符支持',
  'FTXUI 渲染 — 函数式终端 UI 组件',
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

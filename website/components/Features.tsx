import Terminal from './Terminal';

const FEATURES = [
  '拼音输入 — 中文拼音输入法,候选词智能排序',
  '零依赖 — 完全静态链接,下载即用,无需安装任何系统库',
  '自包含构建 — libuv/librime 全部源码内置',
  '完整中文支持 — UTF-8 编解码,CJK 宽字符对齐',
  '终端 UI — 候选栏、状态栏、设置面板,函数式渲染',
];

const USE_CASES = [
  '麒麟 V10 / UOS / 方德 Server 版,选无桌面最小化安装时,纯控制台(tty1–tty6)与裸 SSH shell 下无任何中文输入能力,必须装桌面环境。term-ime 不需要桌面、不需要 D-Bus、不需要 X,编译一个二进制文件传进去就能在终端里输入中文。',
  'ARM64 / LoongArch / SW64 架构的国产服务器,即使没有桌面环境,通过 SSH 远程连接也能用 term-ime 输入中文备注和文档。',
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

        <h2>使用场景</h2>
        <Terminal title="scenarios.txt">
          {USE_CASES.map((f, i) => (
            <div key={i} className="line">
              <span className="bullet">▸</span> {f}
            </div>
          ))}
        </Terminal>
      </div>
    </section>
  );
}

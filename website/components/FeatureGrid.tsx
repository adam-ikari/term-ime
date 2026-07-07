import Terminal from './Terminal';
import {
  IconTerminal,
  IconGlobe,
  IconPuzzle,
  IconBolt,
  IconBrain,
  IconChar,
  IconUi,
} from './icons';

const CARDS = [
  { Icon: IconTerminal, title: 'PTY 虚拟终端', desc: 'forkpty 子进程管理,VT100/CSI 转义序列解析与屏幕缓冲。' },
  { Icon: IconGlobe, title: '多语言输入法', desc: 'librime 封装:拼音、双拼、注音、假名,可按 schema 切换。' },
  { Icon: IconPuzzle, title: '可扩展架构', desc: '语言配置化(LanguageManager),新增语言无需改代码。' },
  { Icon: IconBolt, title: '异步事件驱动', desc: 'libuv 事件循环:fd 轮询、信号、定时器、线程池工作队列。' },
  { Icon: IconBrain, title: '智能候选词', desc: '可选 llama.cpp 异步 LLM 排序,懒加载、过期结果丢弃。' },
  { Icon: IconChar, title: 'UTF-8 + CJK', desc: '完整 UTF-8 编解码,正确处理 CJK 宽字符对齐。' },
  { Icon: IconUi, title: 'FTXUI 渲染', desc: 'JSX 风格声明式组件,候选栏 / 状态栏 / 设置面板。' },
];

export default function FeatureGrid() {
  return (
    <section className="section">
      <div className="container">
        <h2>特性详解</h2>
        <div className="grid">
          {CARDS.map(({ Icon, title, desc }) => (
            <Terminal key={title} title={title} className="card">
              <div className="card-inner">
                <span className="icon">
                  <Icon size={22} />
                </span>
                <p className="desc">{desc}</p>
              </div>
            </Terminal>
          ))}
        </div>
      </div>
    </section>
  );
}

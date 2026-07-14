import Terminal from './Terminal';
import {
  IconGlobe,
  IconPuzzle,
  IconBolt,
  IconChar,
  IconUi,
} from './icons';

const CARDS = [
  { Icon: IconGlobe, title: '拼音输入', desc: '中文拼音输入法,逐字候选,数字键选词,流畅的 TTY 打字体验。' },
  { Icon: IconPuzzle, title: '零依赖', desc: '完全静态链接单文件,下载即用,无需安装任何系统库。' },
  { Icon: IconBolt, title: '自包含构建', desc: 'libuv、librime 全部源码内置,Boost 已彻底剥离,仅需构建工具链。' },
  { Icon: IconChar, title: '完整中文支持', desc: 'UTF-8 编解码,CJK 宽字符正确对齐,Emoji 也不乱码。' },
  { Icon: IconUi, title: '终端 UI', desc: '候选栏、状态栏、设置面板,绿色背景,清爽的终端界面。' },
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

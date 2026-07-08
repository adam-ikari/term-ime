import Terminal from './Terminal';
import {
  IconGlobe,
  IconPuzzle,
  IconBrain,
  IconChar,
  IconUi,
} from './icons';

const CARDS = [
  { Icon: IconGlobe, title: '拼音输入', desc: '中文拼音输入法,逐字候选,数字键选词,流畅的 TTY 打字体验。' },
  { Icon: IconPuzzle, title: '可扩展语言', desc: '语言全部配置驱动,增减语言或切换方案无需改动代码。' },
  { Icon: IconBrain, title: '智能候选词', desc: '可选 AI 排序,根据上下文把最可能的候选词排到最前。' },
  { Icon: IconChar, title: '完整中文支持', desc: 'UTF-8 编解码,CJK 宽字符正确对齐,Emoji 也不乱码。' },
  { Icon: IconUi, title: '终端 UI', desc: '候选栏、状态栏、设置面板,在终端里也有清爽的界面。' },
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

import {type ReactNode, useEffect, useState} from 'react';
import clsx from 'clsx';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import Heading from '@theme/Heading';
import Head from '@docusaurus/Head';

import styles from './index.module.css';

const SITE_URL = 'https://adam-ikari.github.io/term-ime/';
const INSTALL_CMD =
  'curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash';

// AI-SEO: structured data for search engines / AI answer engines (JSON-LD).
const structuredData = {
  '@context': 'https://schema.org',
  '@graph': [
    {
      '@type': 'SoftwareApplication',
      name: 'term-ime',
      applicationCategory: 'UtilityApplication',
      operatingSystem: 'Linux / Unix (TTY, no desktop required)',
      description:
        'SSH 上，终于能打中文了：无 X、无 Wayland、无 D-Bus 的终端中文输入法。输入法引擎库（term-ime-lib，封装 librime）+ TUI 输入法组件（候选栏/状态栏/设置面板），完全静态单文件。',
      url: SITE_URL,
      license: 'https://opensource.org/licenses/MIT',
      offers: { '@type': 'Offer', price: '0', priceCurrency: 'USD' },
      aggregateRating: undefined,
    },
    {
      '@type': 'FAQPage',
      mainEntity: [
        {
          '@type': 'Question',
          name: '什么是 TTY 输入法 / 终端中文输入法？',
          acceptedAnswer: {
            '@type': 'Answer',
            text: 'TTY 输入法是在没有桌面环境的纯终端（Linux TTY、SSH、Docker、WSL）里输入中文的输入法。term-ime 直接读写终端，不需要 X、Wayland、D-Bus 或任何桌面输入法框架。',
          },
        },
        {
          '@type': 'Question',
          name: 'term-ime 可以作为输入法库嵌入其他程序吗？',
          acceptedAnswer: {
            '@type': 'Answer',
            text: '可以。term-ime-lib 是一个独立的静态库，通过极小的 ImeEngine C++ 接口封装 librime，任何 TUI 程序、编辑器插件或终端模拟器都能嵌入获得中文拼音输入能力。',
          },
        },
        {
          '@type': 'Question',
          name: 'term-ime 的 TUI 输入法组件包含什么？',
          acceptedAnswer: {
            '@type': 'Answer',
            text: '候选栏（宽度自适应、去重重绘）、状态栏（独占最后一行）、设置面板（全屏覆盖层，含模糊音 5 组独立开关、候选数量、界面语言）。通过 term-terminal 库复用，带完整 SGR 颜色和 CJK 宽字符对齐。',
          },
        },
        {
          '@type': 'Question',
          name: 'term-ime 支持模糊音吗？',
          acceptedAnswer: {
            '@type': 'Answer',
            text: '支持，且按组独立开关：平翘舌（zh/z）、n/l、r 系、h/f、前后鼻音（en/eng、in/ing、an/ang）。可在设置面板或配置文件（fuzzy_groups）中逐类配置，默认全开。',
          },
        },
        {
          '@type': 'Question',
          name: '预编译包支持哪些架构？',
          acceptedAnswer: {
            '@type': 'Answer',
            text: 'install.sh 只发布 linux-x86_64 预编译包。ARM64、LoongArch、SW64 等架构从源码编译，构建只需要 gcc / cmake 工具链，不需要任何第三方系统库。',
          },
        },
      ],
    },
  ],
};

async function copyText(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch {
    try {
      const ta = document.createElement('textarea');
      ta.value = text;
      ta.style.position = 'fixed';
      ta.style.opacity = '0';
      document.body.appendChild(ta);
      ta.select();
      const ok = document.execCommand('copy');
      document.body.removeChild(ta);
      return ok;
    } catch {
      return false;
    }
  }
}

function CopyButton({
  text,
  label = '复制',
  className,
}: {
  text: string;
  label?: string;
  className?: string;
}) {
  const [copied, setCopied] = useState(false);
  return (
    <button
      type="button"
      className={clsx(styles.copyButton, className)}
      onClick={async () => {
        const ok = await copyText(text);
        if (ok) {
          setCopied(true);
          window.setTimeout(() => setCopied(false), 2000);
        }
      }}
    >
      {copied ? '已复制 ✓' : label}
    </button>
  );
}

/* ---------------------------------------------------------------- demo --- */

type DemoState = {
  pinyin: string;
  output: string;
  cands: string[] | null;
};

const CANDIDATES: Record<string, string[]> = {
  nihao: ['你好', '呢', '你', '泥', '逆'],
  shijie: ['世界', '视界', '事迹', '十一', '是'],
};

function useDemoTyping(): DemoState {
  const [state, setState] = useState<DemoState>({
    pinyin: '',
    output: '',
    cands: null,
  });

  useEffect(() => {
    let cancelled = false;
    const timers: number[] = [];
    const wait = (ms: number) =>
      new Promise<void>((resolve) => {
        timers.push(window.setTimeout(resolve, ms));
      });

    (async () => {
      while (!cancelled) {
        setState({pinyin: '', output: '', cands: null});
        await wait(900);
        for (const ch of 'nihao') {
          if (cancelled) return;
          await wait(170);
          if (cancelled) return;
          setState((s) => {
            const pinyin = s.pinyin + ch;
            return {...s, pinyin, cands: CANDIDATES[pinyin] ?? null};
          });
        }
        await wait(750);
        if (cancelled) return;
        setState({pinyin: '', output: '你好', cands: null});
        await wait(650);
        for (const ch of 'shijie') {
          if (cancelled) return;
          await wait(170);
          if (cancelled) return;
          setState((s) => {
            const pinyin = s.pinyin + ch;
            return {...s, pinyin, cands: CANDIDATES[pinyin] ?? null};
          });
        }
        await wait(750);
        if (cancelled) return;
        setState({pinyin: '', output: '你好世界', cands: null});
        await wait(3400);
      }
    })();

    return () => {
      cancelled = true;
      timers.forEach((t) => window.clearTimeout(t));
    };
  }, []);

  return state;
}

function HeroDemo() {
  const {pinyin, output, cands} = useDemoTyping();
  return (
    <div className={styles.demo} aria-hidden="true">
      <div className={styles.demoChrome}>
        <span className={styles.dots}>
          <span className={styles.dotRed} />
          <span className={styles.dotYellow} />
          <span className={styles.dotGreen} />
        </span>
        <span className={styles.demoTitle}>root@prod-01 — ssh</span>
      </div>
      <div className={styles.demoInner}>
        <div className={styles.demoLineDim}>$ vim 部署说明.md</div>
        <div className={styles.demoLine}>
          <span>{output}</span>
          <span className={styles.cursor} />
        </div>
        <div className={styles.demoLineTilde}>~</div>
        <div className={styles.statusBar}>
          <span className={styles.modeIndicator}>[拼]</span>
          {pinyin && <span className={styles.pinyin}> {pinyin} </span>}
          {cands && (
            <span className={styles.candidates}>
              <span className={styles.candidateSelected}>1.{cands[0]} </span>
              <span className={styles.candidate}>
                {cands.slice(1).map((c, i) => `${i + 2}.${c}`).join(' ')}
              </span>
            </span>
          )}
          {!pinyin && !cands && (
            <span className={styles.statusHint}> Ctrl+A Space 切换中英文</span>
          )}
        </div>
      </div>
    </div>
  );
}

/* ------------------------------------------------------------- sections --- */

function Hero() {
  return (
    <header className={styles.hero}>
      <div className="container">
        <div className={styles.heroInner}>
          <div className={styles.heroText}>
            <div className={styles.badges}>
              <span className={styles.brandBadge}>term-ime</span>
              <span className={styles.badge}>零依赖 · 单文件静态</span>
              <span className={styles.badge}>librime 拼音</span>
              <span className={styles.badge}>MIT</span>
            </div>
            <Heading as="h1" className={styles.heroTitle}>
              SSH 上，终于能打中文了
            </Heading>
            <p className={styles.heroTagline}>
              不装 X，不装 Wayland，不碰 D-Bus。一个完全静态的单文件二进制，
              把 librime 拼音带进 SSH、Docker、WSL 和信创机器的纯终端。
            </p>

            <div className={styles.installBox}>
              <code className={styles.installCmd}>
                <span className={styles.installPrompt}>$ </span>
                {INSTALL_CMD}
              </code>
              <CopyButton text={INSTALL_CMD} label="复制" />
            </div>
            <p className={styles.installNote}>
              装到 ~/.local/bin，不需要 sudo。预编译包为 linux-x86_64，其他架构源码编译。
            </p>

            <div className={styles.buttons}>
              <Link
                className={clsx(
                  'button button--primary button--lg',
                  styles.buttonPrimary
                )}
                to="/docs/quickstart"
              >
                快速开始
              </Link>
              <Link
                className={clsx(
                  'button button--secondary button--lg',
                  styles.buttonSecondary
                )}
                to="https://github.com/adam-ikari/term-ime"
              >
                GitHub
              </Link>
            </div>

            <div className={styles.stats}>
              <div className={styles.stat}>
                <span className={styles.statValue}>0</span>
                <span className={styles.statLabel}>图形栈依赖</span>
              </div>
              <div className={styles.stat}>
                <span className={styles.statValue}>1</span>
                <span className={styles.statLabel}>个文件拷贝即用</span>
              </div>
              <div className={styles.stat}>
                <span className={styles.statValue}>5</span>
                <span className={styles.statLabel}>组模糊音独立开关</span>
              </div>
              <div className={styles.stat}>
                <span className={styles.statValue}>MIT</span>
                <span className={styles.statLabel}>开源协议</span>
              </div>
            </div>
          </div>

          <div className={styles.heroDemo}>
            <HeroDemo />
          </div>
        </div>

        <div className={styles.chips}>
          {[
            'SSH 生产机',
            'Docker 容器',
            'WSL',
            '麒麟 V10 / UOS 控制台',
            '纯 TTY',
            'CI 交互调试',
          ].map((c) => (
            <span key={c} className={styles.chip}>
              {c}
            </span>
          ))}
        </div>
      </div>
    </header>
  );
}

function Pains() {
  const pains = [
    {
      title: 'SSH 上写不了中文',
      desc: '想在 commit message、配置注释里写句话，只能切回桌面打好再粘过来，剪贴板还常常被终端搞乱。',
    },
    {
      title: '信创机器一个汉字都打不出',
      desc: '麒麟 V10 / UOS / 方德 Server 最小化安装，控制台连输入法框架都没有，全靠拼音字母硬凑。',
    },
    {
      title: '容器里配输入法是场噩梦',
      desc: 'X11 转发、D-Bus、fcitx 一串依赖装完还未必能用，镜像大一圈，问题照样复现。',
    },
  ];
  return (
    <section className={styles.section}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          在这些地方，你连一句中文都打不出来
        </Heading>
        <div className={styles.painGrid}>
          {pains.map((p, i) => (
            <div key={p.title} className={styles.painCard}>
              <span className={styles.painIndex}>
                {String(i + 1).padStart(2, '0')}
              </span>
              <Heading as="h3" className={styles.painTitle}>
                {p.title}
              </Heading>
              <p className={styles.painDesc}>{p.desc}</p>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}

function Compare() {
  const rows: [string, string, string, string][] = [
    ['在 SSH 会话里直接打', '否，要切回本地', '勉强，依赖转发与字体', '是'],
    ['需要桌面 / 图形栈', '—', '需要 X、fcitx、X11 转发', '都不需要'],
    ['无桌面的最小系统', '否', '否', '是'],
    ['容器 & WSL', '否', '否', '是'],
    ['安装成本', '—', '多个系统包 + 配置', '一条命令，一个文件'],
  ];
  return (
    <section className={clsx(styles.section, styles.sectionAlt)}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          三种做法，你大概都试过
        </Heading>
        <div className={styles.tableWrap}>
          <table className={styles.compareTable}>
            <thead>
              <tr>
                <th></th>
                <th>本地复制粘贴</th>
                <th>X11 转发 + 图形输入法</th>
                <th className={styles.colWin}>term-ime</th>
              </tr>
            </thead>
            <tbody>
              {rows.map(([label, a, b, c]) => (
                <tr key={label}>
                  <td className={styles.rowHead}>{label}</td>
                  <td>{a}</td>
                  <td>{b}</td>
                  <td className={styles.colWin}>{c}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
        <p className={styles.tableNote}>
          term-ime 直接读写终端字符流，不经过任何图形输入法框架 ——
          所以它在没有桌面的地方照样工作。
        </p>
      </div>
    </section>
  );
}

function QuickStart() {
  const steps = [
    {
      title: '安装',
      copy: INSTALL_CMD,
      code: `# 免 sudo，装到 ~/.local/bin\n${INSTALL_CMD}`,
    },
    {
      title: '运行',
      copy: 'term-ime',
      code: `# 需要真实 TTY 或支持 alternate screen 的终端\nterm-ime`,
    },
    {
      title: '打字',
      copy: '',
      code: `# Ctrl+A 然后 Space 切中英文\n# 输拼音 nihao，按 1 或空格上屏「你好」`,
    },
  ];
  return (
    <section className={styles.section}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          三条命令开始
        </Heading>
        <div className={styles.steps}>
          {steps.map((s, i) => (
            <div key={s.title} className={styles.stepCard}>
              <div className={styles.stepHead}>
                <span className={styles.stepNum}>{i + 1}</span>
                <span className={styles.stepTitle}>{s.title}</span>
                {s.copy && (
                  <CopyButton
                    text={s.copy}
                    label="复制"
                    className={styles.stepCopy}
                  />
                )}
              </div>
              <pre className={styles.stepCode}>
                <code>{s.code}</code>
              </pre>
            </div>
          ))}
        </div>
        <p className={styles.tableNote}>
          完整说明见 <Link to="/docs/quickstart">快速开始</Link>，
          快捷键见 <Link to="/docs/shortcuts">快捷键</Link>。
        </p>
      </div>
    </section>
  );
}

function Features() {
  const features = [
    {
      title: 'librime 拼音',
      desc: '跑的是正经 Rime 引擎，词库与候选排序和桌面版同源，不是玩具级的逐字匹配。',
    },
    {
      title: '5 组模糊音',
      desc: '平翘舌、n/l、r 系、h/f、前后鼻音逐组开关，默认全开，全关即精确拼音。',
    },
    {
      title: '静态单文件',
      desc: 'ldd 显示 not a dynamic executable，没有任何 .so 依赖，拷过去就能跑。',
    },
    {
      title: '自适应候选栏',
      desc: '按终端宽度只显示放得下的候选，绝无半截词；逗号、句号成组翻页。',
    },
    {
      title: '状态栏 + 设置面板',
      desc: '状态栏独占最后一行；Ctrl+A S 打开设置，调候选数、界面语言、模糊音。',
    },
    {
      title: '中文不把界面撑歪',
      desc: 'CJK 宽字符右半格对齐、SGR 16 色、ED/EL 擦除，中文和颜色都规规矩矩。',
    },
  ];
  return (
    <section className={clsx(styles.section, styles.sectionAlt)}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          它做对了哪些事
        </Heading>
        <div className={styles.featureGrid}>
          {features.map((f) => (
            <div key={f.title} className={styles.featureCard}>
              <Heading as="h3" className={styles.featureCardTitle}>
                {f.title}
              </Heading>
              <p className={styles.featureCardDesc}>{f.desc}</p>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}

function Embed() {
  const libs = [
    {
      title: 'term-ime-lib',
      subtitle: '输入法引擎库',
      desc: '极小的 ImeEngine C++ 接口封装 librime。TUI 程序、编辑器插件、终端模拟器嵌入它就能获得拼音输入能力。',
      to: '/docs/library',
      linkLabel: '输入法库文档',
    },
    {
      title: 'term-terminal',
      subtitle: 'TUI 输入法组件',
      desc: '候选栏、状态栏、设置面板三个现成组件，自带 SGR 颜色与 CJK 对齐，接上引擎就是完整输入界面。',
      to: '/docs/tui-component',
      linkLabel: 'TUI 组件文档',
    },
  ];
  return (
    <section className={styles.section}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          想给自己的 TUI 程序加中文输入？
        </Heading>
        <div className={styles.embedGrid}>
          {libs.map((l) => (
            <div key={l.title} className={styles.embedCard}>
              <div className={styles.embedHead}>
                <span className={styles.embedTitle}>{l.title}</span>
                <span className={styles.embedSub}>{l.subtitle}</span>
              </div>
              <p className={styles.embedDesc}>{l.desc}</p>
              <Link className={styles.embedLink} to={l.to}>
                {l.linkLabel} →
              </Link>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}

function Faq() {
  const faqs: [string, ReactNode][] = [
    [
      '什么是 TTY 输入法 / 终端中文输入法？',
      '在没有桌面环境的纯终端里输入中文的输入法。term-ime 直接读写终端字符流，不需要 X、Wayland、D-Bus 或任何桌面输入法框架，所以 SSH、容器、最小化安装的控制台都能用。',
    ],
    [
      '预编译包支持哪些架构？',
      <>
        install.sh 只发布 linux-x86_64 预编译包。ARM64、LoongArch、SW64
        从源码编译，构建只需要 gcc / cmake 工具链，yaml-cpp、leveldb、marisa、opencc
        都随源码静态编译，不需要装任何 <code>-dev</code> 包。
      </>,
    ],
    [
      'term-ime 可以作为输入法库嵌入其他程序吗？',
      '可以。term-ime-lib 是独立静态库，通过 ImeEngine 接口封装 librime，任何 TUI 程序、编辑器插件或终端模拟器都能嵌入获得拼音输入能力。',
    ],
    [
      'term-ime 的 TUI 输入法组件包含什么？',
      '候选栏（宽度自适应、去重重绘）、状态栏（独占最后一行）、设置面板（全屏覆盖层，含 5 组模糊音开关、候选数量、界面语言），通过 term-terminal 库复用。',
    ],
    [
      'term-ime 支持模糊音吗？',
      '支持，且按组独立开关：平翘舌（zh/z）、n/l、r 系、h/f、前后鼻音（en/eng、in/ing、an/ang）。在设置面板或配置文件 fuzzy_groups 里逐类配置，默认全开。',
    ],
  ];
  return (
    <section className={clsx(styles.section, styles.sectionAlt)}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          常见问题
        </Heading>
        <div className={styles.faqList}>
          {faqs.map(([q, a]) => (
            <details key={q} className={styles.faqItem}>
              <summary className={styles.faqQ}>{q}</summary>
              <p className={styles.faqA}>{a}</p>
            </details>
          ))}
        </div>
      </div>
    </section>
  );
}

function ShareLinkButton() {
  const [copied, setCopied] = useState(false);
  return (
    <button
      type="button"
      className={clsx(
        'button button--primary button--lg',
        styles.buttonPrimary
      )}
      onClick={async () => {
        const url =
          typeof window !== 'undefined' ? window.location.href : SITE_URL;
        const ok = await copyText(url);
        if (ok) {
          setCopied(true);
          window.setTimeout(() => setCopied(false), 2000);
        }
      }}
    >
      {copied ? '链接已复制 ✓' : '复制本站链接'}
    </button>
  );
}

function ShareCta() {
  return (
    <section className={styles.share}>
      <div className="container">
        <Heading as="h2" className={styles.shareTitle}>
          转给那个还在切屏复制中文的同事
        </Heading>
        <p className={styles.shareDesc}>
          一条命令，一个文件，不用桌面也能打中文。
        </p>
        <div className={styles.shareButtons}>
          <ShareLinkButton />
          <CopyButton
            text={INSTALL_CMD}
            label="复制安装命令"
            className={styles.shareCopy}
          />
          <Link
            className={clsx(
              'button button--secondary button--lg',
              styles.buttonSecondary
            )}
            to="https://github.com/adam-ikari/term-ime"
          >
            GitHub 点个 Star
          </Link>
        </div>
      </div>
    </section>
  );
}

export default function Home(): ReactNode {
  const {siteConfig} = useDocusaurusContext();
  return (
    <Layout
      title={siteConfig.title}
      description="SSH 上，终于能打中文了：无 X、无 Wayland、无 D-Bus 的终端中文输入法。librime 拼音 + 候选栏/状态栏 TUI 组件，完全静态单文件，一条命令安装。">
      <Head>
        <script type="application/ld+json">{JSON.stringify(structuredData)}</script>
      </Head>
      <Hero />
      <main>
        <Pains />
        <Compare />
        <QuickStart />
        <Features />
        <Embed />
        <Faq />
        <ShareCta />
      </main>
    </Layout>
  );
}

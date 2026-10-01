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

const HERO_TAGLINE =
  'SSH 上改配置、写 commit，写句中文得回桌面，打完再粘回来。term-ime 把输入法装进终端，没有桌面的机器也能用。';

// 首页 FAQ 与 JSON-LD 共用同一份数据：页面显示什么，搜索引擎就看到什么。
const FAQS: [string, string][] = [
  [
    '什么是终端输入法？',
    '在没有桌面的纯终端里打中文的输入法。term-ime 直接读写终端字符流，SSH、容器、最小化安装的控制台都能用。',
  ],
  [
    '预编译包支持哪些架构？',
    '预编译包提供 x86_64 与 aarch64 两种架构，安装脚本按 uname -m 自动识别。其他架构（LoongArch、riscv64 等）请从源码编译，只需要 gcc / cmake 工具链，不装任何 -dev 包。',
  ],
  [
    '手机上能用吗？',
    '可以，提供 arm64 Android 的测试版本。全新 Termux 里没有 curl，先跑一次 pkg install curl，之后和 Linux 用同一条安装命令，脚本自动识别平台，详见 Termux 页面。',
  ],
  [
    '能嵌进我自己的程序吗？',
    '能。term-ime-lib 用 ImeEngine 接口封装 librime，term-terminal 提供候选栏、状态栏、设置面板，TUI 程序接上就能打拼音。',
  ],
  [
    'term-ime 支持模糊音吗？',
    '支持，平翘舌、n/l、r 系、h/f、前后鼻音各有开关，在设置面板或配置文件 fuzzy_groups 里逐组切换，默认全开。',
  ],
  [
    '安装后的命令是什么？',
    '主命令是短命令 ti，term-ime 为兼容别名。项目名是 term-ime，不是 term-time。',
  ],
];

// AI-SEO: structured data for search engines / AI answer engines (JSON-LD).
const structuredData = {
  '@context': 'https://schema.org',
  '@graph': [
    {
      '@type': 'SoftwareApplication',
      name: 'term-ime',
      applicationCategory: 'UtilityApplication',
      operatingSystem: 'Linux (TTY, no desktop required); Android arm64 via Termux',
      description:
        '终端输入法：输入法引擎库（term-ime-lib，封装 librime）加 TUI 输入法组件（候选栏/状态栏/设置面板），Linux 上是零依赖的静态单文件，直接读写终端字符流。',
      url: SITE_URL,
      license: 'https://opensource.org/licenses/MIT',
      offers: { '@type': 'Offer', price: '0', priceCurrency: 'USD' },
      aggregateRating: undefined,
    },
    {
      '@type': 'FAQPage',
      mainEntity: FAQS.map(([q, a]) => ({
        '@type': 'Question',
        name: q,
        acceptedAnswer: {'@type': 'Answer', text: a},
      })),
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
              <span className={styles.badge}>零依赖 · 单文件</span>
              <span className={styles.badge}>librime 拼音</span>
              <span className={styles.badge}>MIT</span>
            </div>
            <Heading as="h1" className={styles.heroTitle}>
              终端输入法
            </Heading>
            <p className={styles.heroTagline}>{HERO_TAGLINE}</p>

            <div className={styles.installBox}>
              <code className={styles.installCmd}>
                <span className={styles.installPrompt}>$ </span>
                {INSTALL_CMD}
              </code>
              <CopyButton text={INSTALL_CMD} label="复制" />
            </div>
            <p className={styles.installNote}>
              免 sudo，装到 ~/.local/bin；其他装法见
              <Link to="/docs/quickstart">快速开始</Link>。
              <br />
              手机上用 Termux：先 <code>pkg install curl</code>（全新 Termux
              没带下载工具），之后同一条命令，脚本自动识别——见{' '}
              <Link to="/docs/termux">Termux 页面</Link>。
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
                <span className={styles.statLabel}>运行时依赖（Linux）</span>
              </div>
              <div className={styles.stat}>
                <span className={styles.statValue}>1</span>
                <span className={styles.statLabel}>个文件装完即用</span>
              </div>
              <div className={styles.stat}>
                <span className={styles.statValue}>70655</span>
                <span className={styles.statLabel}>词组词库</span>
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
          <span className={styles.chipsLabel}>用在哪</span>
          {[
            'SSH 生产机',
            'Docker 容器',
            'WSL',
            '无桌面的 Linux server',
            '纯 TTY',
            'CI 交互调试',
            'Android / Termux（测试版）',
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

function Why() {
  const pains = [
    {
      title: '写中文得来回切屏',
      desc: '在本地窗口打好再粘回来，剪贴板还常常被终端搞乱。',
    },
    {
      title: '服务器上压根没有输入法',
      desc: '最小化安装没有桌面，也没有输入法框架，中文注释只能拿拼音字母硬凑。',
    },
    {
      title: '容器里装输入法太费劲',
      desc: 'X11 转发、D-Bus、fcitx 装一大堆，镜像大一圈，装完还未必能用。',
    },
  ];
  const rows: [string, string, string, string][] = [
    ['SSH 会话里直接打字', '否，得切回本地', '勉强，依赖转发和字体', '可以'],
    ['需要桌面 / 图形栈', '不适用', 'X、fcitx、X11 转发', '都不需要'],
    ['无桌面的最小系统', '否', '否', '可以'],
    ['容器 & WSL', '否', '否', '可以'],
    ['安装成本', '不适用', '多个系统包 + 配置', '一条命令，一个文件'],
  ];
  return (
    <section className={styles.section}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          没有输入法的终端有多难用
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

        <Heading as="h3" className={styles.subTitle}>
          跟复制粘贴、X11 转发比
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
          term-ime 直接读写终端字符流，不经过图形输入法框架。
        </p>
      </div>
    </section>
  );
}

function QuickStart() {
  const steps = [
    {
      title: '启动',
      copy: 'ti',
      code: `# 短命令 ti（term-ime 为兼容别名）\nti`,
    },
    {
      title: '切中文',
      copy: '',
      code: `# Ctrl+A 然后 Space，中英文之间来回切\nCtrl+A  Space`,
    },
    {
      title: '打字上屏',
      copy: '',
      code: `# 输拼音，按 1 或空格上屏；逗号句号翻页\nnihao  →  你好`,
    },
  ];
  return (
    <section className={clsx(styles.section, styles.sectionAlt)}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          装完怎么用
        </Heading>
        <div className={styles.steps}>
          {steps.map((s, i) => (
            <div key={s.title} className={styles.stepCard}>
              <Heading as="h3" className={styles.stepTitle}>
                {i + 1}. {s.title}
              </Heading>
              <div className={styles.stepCodeWrap}>
                <pre className={styles.stepCode}>
                  <code>{s.code}</code>
                </pre>
                {s.copy && (
                  <CopyButton
                    text={s.copy}
                    label="复制"
                    className={styles.stepCopy}
                  />
                )}
              </div>
            </div>
          ))}
        </div>
      <p className={styles.tableNote}>
        完整按键见<Link to="/docs/shortcuts">快捷键</Link>，其他装法见
        <Link to="/docs/quickstart">快速开始</Link>。
      </p>
      </div>
    </section>
  );
}

function Features() {
  const features = [
    {
      title: 'librime 拼音',
      desc: '词库和候选排序跟桌面版 Rime 同源。',
    },
    {
      title: '模糊音',
      desc: '平翘舌、n/l、r 系、h/f、前后鼻音，一组一个开关，默认全开。',
    },
    {
      title: '单文件',
      desc: '一个文件就是全部，拷到别的机器照样跑。',
    },
    {
      title: '自适应候选栏',
      desc: '窄终端只显示放得下的候选，逗号句号翻页。',
    },
    {
      title: '状态栏和设置面板',
      desc: '状态栏占最后一行，Ctrl+A S 打开设置，调候选数和模糊音。',
    },
    {
      title: '中文不错位',
      desc: 'CJK 宽字符对齐，SGR 颜色，重绘不会留半个字。',
    },
  ];
  const libs = [
    {
      title: 'term-ime-lib',
      subtitle: '输入法引擎库',
      desc: 'ImeEngine 接口封装 librime，TUI 程序、编辑器插件嵌进来就有拼音输入。',
      to: '/docs/library',
      linkLabel: '输入法库文档',
    },
    {
      title: 'term-terminal',
      subtitle: 'TUI 输入法组件',
      desc: '候选栏、状态栏、设置面板三个现成组件，自带颜色和中文对齐。',
      to: '/docs/tui-component',
      linkLabel: 'TUI 组件文档',
    },
  ];
  return (
    <section className={styles.section}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          功能
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

        <Heading as="h3" className={styles.subTitle}>
          嵌进你自己的程序
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
                {l.linkLabel}
              </Link>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}

function Faq() {
  return (
    <section className={clsx(styles.section, styles.sectionAlt)}>
      <div className="container">
        <Heading as="h2" className={styles.sectionTitle}>
          常见问题
        </Heading>
        <div className={styles.faqList}>
          {FAQS.map(([q, a]) => (
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
      description="终端输入法：librime 拼音加候选栏/状态栏 TUI 组件，静态单文件，一条命令安装，SSH、容器和无桌面的服务器通用。">
      <Head>
        <script type="application/ld+json">{JSON.stringify(structuredData)}</script>
      </Head>
      <Hero />
      <main>
        <Why />
        <QuickStart />
        <Features />
        <Faq />
        <ShareCta />
      </main>
    </Layout>
  );
}

import type { Metadata } from 'next';
import localFont from 'next/font/local';
import './globals.css';

// Self-hosted Maple Mono NF CN (Nerd Font + CJK, subset to ASCII + CJK +
// punctuation + Nerd Font PUA glyphs, woff2). Loaded via next/font/local so
// it works on GitHub Pages static export.
const mono = localFont({
  src: [
    {
      path: '../public/fonts/MapleMono-NF-CN-Regular.woff2',
      weight: '400',
      style: 'normal',
    },
    {
      path: '../public/fonts/MapleMono-NF-CN-Bold.woff2',
      weight: '700',
      style: 'normal',
    },
  ],
  variable: '--font-mono',
  display: 'swap',
});

export const metadata: Metadata = {
  title: 'term-ime — 终端中文输入法',
  description: '在终端里直接输入中文。SSH 远程服务器、Docker 容器、WSL、国产操作系统(麒麟/UOS/方德)均可使用。无需桌面环境,无需 D-Bus,无需 X。',
  metadataBase: new URL('https://adam-ikari.github.io'),
  keywords: ['终端中文输入法', 'Linux 中文输入', 'SSH 中文输入', 'Docker 中文输入', 'WSL 中文输入', '国产服务器', '麒麟', '统信', '方德', 'ARM64', 'LoongArch', 'SW64', 'term-ime'],
  openGraph: {
    title: 'term-ime — 终端中文输入法',
    description: '在终端里直接输入中文。SSH 远程服务器、Docker 容器、WSL、国产操作系统均可使用。',
    type: 'website',
    locale: 'zh_CN',
    siteName: 'term-ime',
  },
  twitter: {
    card: 'summary',
    title: 'term-ime — 终端中文输入法',
    description: '在终端里直接输入中文。SSH 远程服务器、Docker 容器、WSL 均可使用。',
  },
  robots: {
    index: true,
    follow: true,
  },
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="zh-CN" className={mono.variable}>
      <body>{children}</body>
    </html>
  );
}

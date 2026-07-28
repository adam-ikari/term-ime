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
  title: 'term-ime — TTY 中文输入虚拟终端',
  description: '在 Linux TTY 中运行的虚拟终端,内置中文拼音输入法,自包含、无系统输入法依赖。支持 ARM64/LoongArch/SW64 国产服务器,麒麟/UOS/方德等操作系统。',
  metadataBase: new URL('https://adam-ikari.github.io'),
  keywords: ['中文输入法', 'Linux TTY', '终端输入法', 'SSH 中文输入', '国产服务器', '麒麟', '统信', '方德', 'ARM64', 'LoongArch', 'SW64', 'term-ime'],
  openGraph: {
    title: 'term-ime — Linux TTY 中文输入法',
    description: 'SSH 远程连接服务器,在纯终端环境中直接输入中文,无需桌面依赖。支持国产 CPU 架构。',
    type: 'website',
    locale: 'zh_CN',
    siteName: 'term-ime',
  },
  twitter: {
    card: 'summary',
    title: 'term-ime — Linux TTY 中文输入法',
    description: 'SSH 远程连接服务器,在纯终端环境中直接输入中文,无需桌面依赖。',
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

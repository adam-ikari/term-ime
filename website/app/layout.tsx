import type { Metadata } from 'next';
import localFont from 'next/font/local';
import './globals.css';

// Self-hosted Maple Mono CN (sub set to ASCII + CJK + punctuation, woff2).
// Loaded via next/font/local so it works on GitHub Pages static export.
const mono = localFont({
  src: [
    {
      path: '../public/fonts/MapleMono-CN-Regular.woff2',
      weight: '400',
      style: 'normal',
    },
    {
      path: '../public/fonts/MapleMono-CN-Bold.woff2',
      weight: '700',
      style: 'normal',
    },
  ],
  variable: '--font-mono',
  display: 'swap',
});

export const metadata: Metadata = {
  title: 'term-ime — TTY 中文输入虚拟终端',
  description: '在 Linux TTY 中运行的虚拟终端,内置多语言输入法与可选 AI 候选词排序。',
  metadataBase: new URL('https://adam.github.io'),
  openGraph: {
    title: 'term-ime',
    description: 'Linux TTY 虚拟终端,内置多语言输入法',
    type: 'website',
  },
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="zh-CN" className={mono.variable}>
      <body>{children}</body>
    </html>
  );
}

import type { Metadata } from 'next';
import { JetBrains_Mono } from 'next/font/google';
import './globals.css';

const mono = JetBrains_Mono({
  subsets: ['latin'],
  variable: '--font-jbmono',
  display: 'swap',
});

export const metadata: Metadata = {
  title: 'term-ime — TTY 中文输入虚拟终端',
  description:
    '在 Linux TTY 中运行的虚拟终端,内置多语言输入法 (librime) 与可选 LLM 候选词排序 (llama.cpp)。',
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

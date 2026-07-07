import Hero from '@/components/Hero';
import Features from '@/components/Features';
import FeatureGrid from '@/components/FeatureGrid';
import QuickStart from '@/components/QuickStart';
import Shortcuts from '@/components/Shortcuts';
import Footer from '@/components/Footer';
import { IconGitHub } from '@/components/icons';

export default function Home() {
  return (
    <>
      <nav className="nav">
        <div className="container">
          <a className="brand" href="#hero">
            term-ime
          </a>
          <span className="links">
            <a href="#features">特性</a>
            <a href="#quickstart">快速开始</a>
            <a href="#shortcuts">快捷键</a>
            <a href="/docs/">文档</a>
            <a
              href="https://github.com/adam-ikari/term-ime"
              target="_blank"
              rel="noopener noreferrer"
              aria-label="GitHub"
            >
              <IconGitHub size={16} />
            </a>
          </span>
        </div>
      </nav>

      <main>
        <Hero />
        <Features />
        <FeatureGrid />
        <QuickStart />
        <Shortcuts />
        <Footer />
      </main>
    </>
  );
}

'use client';

import { useEffect, useRef } from 'react';
import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';
import { DEMO_SCRIPT, lookupCandidates, type Candidate } from '@/lib/demo-script';

// ANSI color helpers — match term-ime's REAL palette (24-bit true color).
// The real status bar uses FTXUI BgColor which paints the entire row; we
// simulate by padding spaces to term.cols and wrapping in barBg.
const C = {
  reset: '\x1b[0m',
  barBg: '\x1b[48;2;22;101;52m',         // deep green status bar background
  prompt: '\x1b[1;32m',                  // $ prompt
  shellOut: '\x1b[0m',                   // committed shell text
};

type Props = { className?: string };

/**
 * A looping, auto-playing term-ime demo rendered with xterm.js (a real
 * terminal emulator). No controls — it cycles forever: toggle to 中文, type
 * pinyin, show the candidate bar, commit a candidate, repeat. Pure client-side.
 */
export default function TerminalDemo({ className = '' }: Props) {
  const hostRef = useRef<HTMLDivElement>(null);
  const termRef = useRef<Terminal | null>(null);

  useEffect(() => {
    if (!hostRef.current) return;
    const term = new Terminal({
      fontFamily: "'Maple Mono NF CN','Maple Mono NF','JetBrains Mono',ui-monospace,monospace",
      fontSize: 14,
      lineHeight: 1.4,
      cursorBlink: true,
      cursorStyle: 'block',
      allowTransparency: true,
      scrollback: 0,
      convertEol: true,
      theme: {
        background: '#1a1a1a',
        foreground: '#e5e5e5',
        cursor: '#00ff9c',
        selectionBackground: '#003b2a',
      },
    });
    const fit = new FitAddon();
    term.loadAddon(fit);
    term.open(hostRef.current);
    try {
      fit.fit();
    } catch {
      /* host not laid out yet; ignore */
    }
    termRef.current = term;

    let stopped = false;
    let timer: ReturnType<typeof setTimeout>;

    const wait = (ms: number) => new Promise<void>((r) => (timer = setTimeout(r, ms)));

    // Render one frame of the term-ime screen into the xterm. Mirrors the real
    // app's layout: a shell prompt line, blank line, then the single status /
    // candidate bar on the last line with reverse-video background.
    function render(opts: {
      mode: 'EN' | 'CN';
      output: string;
      composition: string;
      candidates: Candidate[];
      selectedIdx: number | null;
    }) {
      const { mode, output, composition, candidates, selectedIdx } = opts;
      term.reset();
      // shell line: `$ ` + committed output + cursor
      term.writeln(`${C.prompt}$ ${C.shellOut}${output}${C.reset}`);
      term.writeln('');

      // status / candidate bar (green background, single line, full width)
      const modeLabel = mode === 'CN' ? '中文' : 'EN';
      let bar = '';
      if (mode === 'CN' && composition) {
        const displayComp = composition
          .replace(/^(n[hi]|ni|na|ne|n[a-z]?)/, '$1 ')
          .replace(/(h[ao])/g, ' $1')
          .replace(/\s+/g, ' ')
          .trim();
        bar = ` [${modeLabel}]  拼音: ${displayComp} `;
        bar += candidates
          .map((c, i) => {
            const sel = selectedIdx === i;
            const body = `${c.key}.${c.text}`;
            return sel ? ` [${body}] ` : ` ${body} `;
          })
          .join('');
      } else {
        bar = ` [${modeLabel}]  ^A Space 切换 | ^A S 设置`;
      }
      // Pad to fill the terminal width with green background (like the real
      // FTXUI BgColor that paints the entire row), using xterm's current cols.
      const cols = term.cols;
      // Strip ANSI escapes to get the visual width
      const plain = bar.replace(/\x1b\[[0-9;]*m/g, '');
      const padLen = Math.max(0, cols - plain.length);
      bar = bar + ' '.repeat(padLen);
      term.writeln(`${C.barBg}${bar}${C.reset}`);
    }

    async function run() {
      while (!stopped) {
        let mode: 'EN' | 'CN' = 'EN';
        let output = '';
        let composition = '';
        let candidates: Candidate[] = [];
        let selectedIdx: number | null = null;

        const setComp = (c: string) => {
          composition = c;
          candidates = lookupCandidates(c);
        };

        for (const step of DEMO_SCRIPT) {
          if (stopped) return;
          if (step.kind === 'toggle') {
            mode = mode === 'EN' ? 'CN' : 'EN';
            composition = '';
            candidates = [];
            selectedIdx = null;
            render({ mode, output, composition, candidates, selectedIdx });
            await wait(500);
          } else if (step.kind === 'wait') {
            await wait(step.ms);
          } else if (step.kind === 'select') {
            selectedIdx = step.index;
            render({ mode, output, composition, candidates, selectedIdx });
            await wait(280);
            const c = candidates[step.index];
            if (c) output += c.text;
            composition = '';
            candidates = [];
            selectedIdx = null;
            render({ mode, output, composition, candidates, selectedIdx });
            await wait(400);
          } else if (step.kind === 'type') {
            for (const ch of step.chars) {
              if (stopped) return;
              if (mode !== 'CN') {
                output += ch;
              } else {
                setComp(composition + ch);
              }
              render({ mode, output, composition, candidates, selectedIdx });
              await wait(130);
            }
          }
        }
        // pause, then loop
        await wait(1800);
      }
    }

    run();

    const onResize = () => {
      try {
        fit.fit();
      } catch {
        /* ignore */
      }
    };
    window.addEventListener('resize', onResize);

    return () => {
      stopped = true;
      clearTimeout(timer);
      window.removeEventListener('resize', onResize);
      term.dispose();
      termRef.current = null;
    };
  }, []);

  return <div className={`xterm-host ${className}`} ref={hostRef} />;
}

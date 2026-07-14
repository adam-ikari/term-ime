'use client';

import { useEffect, useRef } from 'react';
import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';
import { DEMO_SCRIPT, lookupCandidates, type Candidate } from '@/lib/demo-script';

// ANSI color helpers — match term-ime's REAL palette (24-bit true color):
//   status bar background = deep green RGB(22,101,52)
//   mode text [中文]/[EN]  = white RGB(235,245,240) on green
//   selected candidate     = dark green on bright green RGB(74,222,128)
//   other candidates       = light yellow RGB(254,240,138) on green
//   hints                  = soft green-white RGB(167,219,191)
const C = {
  reset: '\x1b[0m',
  text: '\x1b[1m',                       // bold white (拼音 + committed output)
  plain: '\x1b[0m',
  modeCN: '\x1b[38;2;235;245;240m',      // white mode text (same for CN/EN)
  modeEN: '\x1b[38;2;235;245;240m',      // white mode text
  bracket: '\x1b[38;2;167;219;191m',     // soft green-white for brackets
  sel: '\x1b[1;38;2;20;83;45;48;2;74;222;128m',  // dark green on bright green
  cand: '\x1b[38;2;254;240;138m',        // light yellow on green bg
  hint: '\x1b[2;38;2;167;219;191m',      // soft green-white
  prompt: '\x1b[1;32m',                  // $ prompt
  barBg: '\x1b[48;2;22;101;52m',         // deep green status bar background
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
      cols: 80,
      rows: 8,
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

      // status / candidate bar (green background, single line)
      const modeColor = mode === 'CN' ? C.modeCN : C.modeEN;
      const modeLabel = mode === 'CN' ? '中文' : 'EN';
      let bar = `${C.barBg}${C.bracket} [${modeColor}${modeLabel}${C.bracket}]`;
      if (mode === 'CN' && composition) {
        // Split composition into syllables for display (e.g. "nihao" -> "ni hao")
        const displayComp = composition
          .replace(/^(n[hi]|ni|na|ne|n[a-z]?)/, '$1 ')
          .replace(/(h[ao])/g, ' $1')
          .replace(/\s+/g, ' ')
          .trim();
        bar += ` ${C.text}拼音: ${displayComp}${C.plain}`;
        bar += candidates
          .map((c, i) => {
            const sel = selectedIdx === i;
            const body = `${c.key}.${c.text}`;
            return sel
              ? ` ${C.sel}[${body}]${C.barBg} `
              : ` ${C.cand}${body}`;
          })
          .join('');
        // pad to fill the line with green background
        bar += `${C.barBg}  `;
      } else {
        bar += ` ${C.hint}^A Space 切换 | ^A S 设置`;
      }
      bar += `${C.reset}`;
      term.writeln(bar);
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

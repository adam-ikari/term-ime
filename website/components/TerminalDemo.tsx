'use client';

import { useEffect, useRef } from 'react';
import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';
import { DEMO_SCRIPT, lookupCandidates, type Candidate } from '@/lib/demo-script';

// ANSI color helpers — match term-ime's real palette (256-color, observed
// via term-debug-mcp on the live app):
//   【中文】 = bright green (1;32), 【EN】 = bright cyan (1;36)
//   拼音: <buf>  = bold (1)
//   selected candidate [N.x] = yellow on blue (1;33;44)
//   other candidates       = dim yellow (22;33)
//   Esc 取消 / hints       = dim bright-black (2;90)
//   status bar background  = reverse video (7m), like the real app.
const C = {
  reset: '\x1b[0m',
  text: '\x1b[1m',                       // bold white (拼音 + committed output)
  plain: '\x1b[0m',
  modeCN: '\x1b[1;32m',                  // 【中文】 bright green
  modeEN: '\x1b[1;36m',                  // 【EN】 bright cyan
  sel: '\x1b[1;33;44m',                  // selected candidate: bold yellow on blue
  cand: '\x1b[22;33m',                   // other candidates: dim yellow
  hint: '\x1b[2;90m',                    // Esc 取消 / hints
  prompt: '\x1b[1;32m',                  // $ prompt
  rev: '\x1b[7m',                        // reverse video (status bar bg)
  ai: '\x1b[1;33m',                      // [AI] badge
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
      aiBadge: boolean;
    }) {
      const { mode, output, composition, candidates, selectedIdx, aiBadge } = opts;
      term.reset();
      // shell line: `$ ` + committed output + cursor
      term.writeln(`${C.prompt}$ ${C.shellOut}${output}${C.reset}`);
      term.writeln('');

      // status / candidate bar (reverse video, single line)
      const modeColor = mode === 'CN' ? C.modeCN : C.modeEN;
      const modeLabel = mode === 'CN' ? '中文' : 'EN';
      let bar = `${C.rev}${C.hint} ${C.reset}${modeColor}【${modeLabel}】${C.reset}`;
      if (mode === 'CN' && composition) {
        bar += ` ${C.text}拼音: ${composition} ${C.reset}`;
        bar += candidates
          .map((c, i) => {
            const sel = selectedIdx === i;
            const body = `${c.key}.${c.text}`;
            return sel
              ? `${C.sel} [${body}]${C.reset} `
              : `${C.cand} ${body} ${C.reset}`;
          })
          .join('');
        bar += `${C.hint} Esc 取消${C.reset}`;
      } else {
        // EN / idle: the real status bar shows the hint line.
        bar += ` ${C.hint} Ctrl+A,Space 切换 | 1-9 选择 | Esc 取消 | Ctrl+A,A AI排序 | Ctrl+A,S 设置${C.reset}`;
      }
      if (aiBadge) bar += `${C.ai} [AI]${C.reset}`;
      bar += `${C.rev}${C.reset}`;  // end reverse-video span
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
            render({ mode, output, composition, candidates, selectedIdx, aiBadge: false });
            await wait(500);
          } else if (step.kind === 'wait') {
            await wait(step.ms);
          } else if (step.kind === 'select') {
            selectedIdx = step.index;
            render({ mode, output, composition, candidates, selectedIdx, aiBadge: false });
            await wait(280);
            const c = candidates[step.index];
            if (c) output += c.text;
            composition = '';
            candidates = [];
            selectedIdx = null;
            render({ mode, output, composition, candidates, selectedIdx, aiBadge: false });
            await wait(400);
          } else if (step.kind === 'type') {
            for (const ch of step.chars) {
              if (stopped) return;
              if (mode !== 'CN') {
                output += ch;
              } else {
                setComp(composition + ch);
              }
              render({ mode, output, composition, candidates, selectedIdx, aiBadge: false });
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

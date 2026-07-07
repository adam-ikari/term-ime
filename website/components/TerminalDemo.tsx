'use client';

import { useEffect, useRef } from 'react';
import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';
import { DEMO_SCRIPT, lookupCandidates, type Candidate } from '@/lib/demo-script';

// ANSI color helpers (truecolor). term-ime's real palette:
//   green #00ff9c (accent), amber #ffb000 (mode/AI), dim #888, cyan #56b6c2 (EN).
const C = {
  reset: '\x1b[0m',
  text: '\x1b[38;2;229;229;229m',
  dim: '\x1b[38;2;136;136;136m',
  faint: '\x1b[38;2;85;85;85m',
  green: '\x1b[38;2;0;255;156m',
  amber: '\x1b[38;2;255;176;0m',
  cyan: '\x1b[38;2;86;182;194m',
  selBg: '\x1b[48;2;0;59;42m',
  bold: '\x1b[1m',
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
      fontFamily: "'JetBrains Mono','Fira Code',ui-monospace,monospace",
      fontSize: 14,
      lineHeight: 1.4,
      cols: 64,
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

    // Render one frame of the term-ime screen into the xterm.
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
      // shell prompt line(s)
      term.writeln(`${C.green}$ ${C.text}${output}${C.reset}`);
      term.writeln('');
      // candidate / status bar (last line)
      const modeColor = mode === 'CN' ? C.amber : C.cyan;
      const modeLabel = mode === 'CN' ? '中文' : 'EN';
      let bar = `${C.bold}${modeColor}【${modeLabel}】${C.reset}${C.text}`;
      if (mode === 'CN' && composition) {
        bar += `  ${C.dim}拼音: ${C.text}${composition} ${C.reset}`;
        bar += candidates
          .map((c, i) => {
            const sel = selectedIdx === i;
            const body = `${c.key}.${c.text}`;
            return sel
              ? `${C.selBg}${C.green}${body}${C.reset}${C.text} `
              : `${C.green}${body}${C.reset}${C.dim} `;
          })
          .join('');
        bar += `${C.faint} Esc 取消${C.reset}`;
      }
      if (aiBadge) bar += `${C.amber} [AI]${C.reset}`;
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

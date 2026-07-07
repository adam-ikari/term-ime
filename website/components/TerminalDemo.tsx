'use client';

import { useCallback, useEffect, useRef, useState } from 'react';
import Terminal from './Terminal';
import {
  DEMO_SCRIPT,
  INITIAL_SCENE,
  lookupCandidates,
  type Candidate,
  type DemoScene,
  type ScriptStep,
} from '@/lib/demo-script';

const TYPE_INTERVAL = 120; // ms per char
const SELECT_HIGHLIGHT_MS = 250;
const CYCLE_RESTART_MS = 1500;

export default function TerminalDemo() {
  const [scene, setScene] = useState<DemoScene>(INITIAL_SCENE);
  const [selectedIdx, setSelectedIdx] = useState<number | null>(null);
  const [playing, setPlaying] = useState(true);
  const [reduced, setReduced] = useState(false);

  // Script cursor. stepIdx = current step; charIdx = chars consumed in a 'type' step.
  const stepIdx = useRef(0);
  const charIdx = useRef(0);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  // Detect prefers-reduced-motion.
  useEffect(() => {
    const mq = window.matchMedia('(prefers-reduced-motion: reduce)');
    const update = () => setReduced(mq.matches);
    update();
    mq.addEventListener('change', update);
    return () => mq.removeEventListener('change', update);
  }, []);

  /** Apply a single script step's *start* effect and return how long to wait
   * before advancing to the next step (or null = schedule the next tick). */
  const applyStep = useCallback(
    (step: ScriptStep): number | null => {
      if (step.kind === 'toggle') {
        setScene((s) => {
          const mode = s.mode === 'EN' ? 'CN' : 'EN';
          // Toggling clears composition (mirrors the real F1 fix).
          return { ...s, mode, composition: '', candidates: [] };
        });
        return 0;
      }
      if (step.kind === 'wait') {
        return step.ms;
      }
      if (step.kind === 'select') {
        setScene((s) => {
          const c = s.candidates[step.index];
          if (c) {
            setSelectedIdx(step.index);
            // schedule the actual commit + clear after highlight
            if (timer.current) clearTimeout(timer.current);
            timer.current = setTimeout(() => {
              setScene((cur) => ({
                ...cur,
                output: cur.output + c.text,
                composition: '',
                candidates: [],
              }));
              setSelectedIdx(null);
            }, SELECT_HIGHLIGHT_MS);
          }
          return s; // unchanged now; commit happens in the timeout
        });
        return SELECT_HIGHLIGHT_MS + 50;
      }
      // 'type' — handled by the typing ticker, not here.
      return null;
    },
    []
  );

  /** Advance one step, setting up the next timer. */
  const advance = useCallback(() => {
    if (stepIdx.current >= DEMO_SCRIPT.length) {
      // end of script: restart after a pause if still playing
      if (playing) {
        timer.current = setTimeout(() => {
          stepIdx.current = 0;
          charIdx.current = 0;
          setScene(INITIAL_SCENE);
          setSelectedIdx(null);
          advance();
        }, CYCLE_RESTART_MS);
      }
      return;
    }
    const step = DEMO_SCRIPT[stepIdx.current];

    if (step.kind === 'type') {
      // type one char now; if more chars remain in this step, schedule next tick
      const chars = step.chars;
      if (charIdx.current < chars.length) {
        const ch = chars[charIdx.current++];
        setScene((s) => {
          if (s.mode !== 'CN') {
            // EN mode: pass through to output (shell echo)
            return { ...s, output: s.output + ch };
          }
          const composition = s.composition + ch;
          return {
            ...s,
            composition,
            candidates: lookupCandidates(composition),
          };
        });
        timer.current = setTimeout(advance, reduced ? 0 : TYPE_INTERVAL);
      } else {
        // this step done
        stepIdx.current += 1;
        charIdx.current = 0;
        timer.current = setTimeout(advance, reduced ? 0 : 60);
      }
      return;
    }

    // non-type step
    const wait = applyStep(step);
    stepIdx.current += 1;
    charIdx.current = 0;
    if (wait !== null) {
      timer.current = setTimeout(advance, wait);
    } else {
      timer.current = setTimeout(advance, 0);
    }
  }, [applyStep, playing, reduced]);

  // Drive auto-play.
  useEffect(() => {
    if (!playing) return;
    advance();
    return () => {
      if (timer.current) clearTimeout(timer.current);
    };
  }, [playing, advance]);

  /** User actions: pause auto-play, then mutate scene. */
  const userToggle = useCallback(() => {
    setPlaying(false);
    setScene((s) => {
      const mode = s.mode === 'EN' ? 'CN' : 'EN';
      return { ...s, mode, composition: '', candidates: [] };
    });
    setSelectedIdx(null);
  }, []);

  const userSelect = useCallback((index: number) => {
    setPlaying(false);
    setScene((s) => {
      const c = s.candidates[index];
      if (!c) return s;
      return {
        ...s,
        output: s.output + c.text,
        composition: '',
        candidates: [],
      };
    });
    setSelectedIdx(null);
  }, []);

  const userClear = useCallback(() => {
    setPlaying(false);
    setScene((s) => ({ ...s, composition: '', candidates: [] }));
    setSelectedIdx(null);
  }, []);

  const userReplay = useCallback(() => {
    if (timer.current) clearTimeout(timer.current);
    stepIdx.current = 0;
    charIdx.current = 0;
    setScene(INITIAL_SCENE);
    setSelectedIdx(null);
    setPlaying(true);
  }, []);

  // Keyboard when focused.
  const onKeyDown = useCallback(
    (e: React.KeyboardEvent) => {
      const k = e.key;
      if (k === 'Escape') {
        e.preventDefault();
        userClear();
      } else if (k === ' ') {
        e.preventDefault();
        userSelect(0);
      } else if (k >= '1' && k <= '9') {
        e.preventDefault();
        userSelect(parseInt(k, 10) - 1);
      }
    },
    [userClear, userSelect]
  );

  // Render only the last ~3 "lines" of output (split by space for simplicity).
  const outWords = scene.output.split(' ');
  const tailOut = outWords.slice(-3).join(' ');

  const modeLabel = scene.mode === 'CN' ? '中文' : 'EN';
  const modeColor = scene.mode === 'CN' ? 'cn' : 'en';

  return (
    <section id="demo" className="section demo">
      <div className="container">
        <h2>交互演示</h2>
        <p className="hint">
          点击下方终端区域聚焦后可用键盘(1-9 选词、Space 选首个、Esc 取消)。
          或用按钮操作。自动播放可暂停。
        </p>

        <div
          className="demo-wrap"
          ref={containerRef}
          tabIndex={0}
          onKeyDown={onKeyDown}
          aria-label="term-ime 输入法交互演示"
        >
          <Terminal title="term-ime — demo" className="demo-terminal">
            <div className="screen">
              <div className="shell-line">
                <span className="prompt">$ </span>
                <span className="out">{tailOut}</span>
                {scene.composition && scene.mode === 'CN' && (
                  <span className="comp">{scene.composition}</span>
                )}
                <span className="cursor" />
              </div>
              <div className="cand-bar">
                <span className={`mode ${modeColor}`}>【{modeLabel}】</span>{' '}
                {scene.mode === 'CN' && scene.composition && (
                  <>
                    <span className="pinyin"> 拼音: {scene.composition} </span>
                    <span className="cands">
                      {scene.candidates.map((c: Candidate, i: number) => (
                        <span
                          key={c.key}
                          className={`cand ${selectedIdx === i ? 'sel' : ''}`}
                        >
                          {c.key}.{c.text}{' '}
                        </span>
                      ))}
                    </span>
                    <span className="esc"> Esc 取消</span>
                  </>
                )}
                {scene.aiBadge && (
                  <span className="ai"> [AI]</span>
                )}
              </div>
            </div>
          </Terminal>

          <div className="controls" role="group" aria-label="演示控制">
            <button className="btn kbd-btn" onClick={userToggle}>
              <kbd className="kbd">Ctrl+A</kbd> <kbd className="kbd">Space</kbd>{' '}
              切换中英文
            </button>
            <button
              className="btn kbd-btn"
              onClick={() => userSelect(0)}
              disabled={scene.candidates.length === 0}
            >
              <kbd className="kbd">Space</kbd> 选首个
            </button>
            <button className="btn kbd-btn" onClick={userClear}>
              <kbd className="kbd">Esc</kbd> 取消
            </button>
            <button className="btn kbd-btn" onClick={userReplay}>
              {playing ? '⏸ 暂停重播' : '▶ 重播'}
            </button>
          </div>
        </div>
      </div>
    </section>
  );
}

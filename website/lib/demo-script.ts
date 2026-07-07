// Data and types for the <TerminalDemo> interactive component.
// Pure client-side state — no backend, no network.

export type Candidate = { key: string; text: string };

export type ScriptStep =
  | { kind: 'type'; chars: string } // append these pinyin chars to the composition
  | { kind: 'wait'; ms: number } // pause
  | { kind: 'select'; index: number } // pick candidate[index], commit to output
  | { kind: 'toggle' }; // toggle CN/EN mode (clears composition, mirroring the F1 fix)

export type DemoScene = {
  mode: 'EN' | 'CN';
  composition: string; // pinyin buffer
  candidates: Candidate[]; // shown when composition non-empty in CN mode
  output: string; // committed text so far
  aiBadge: boolean; // show [AI] badge on the status bar
};

export const INITIAL_SCENE: DemoScene = {
  mode: 'EN',
  composition: '',
  candidates: [],
  output: '',
  aiBadge: false,
};

// Candidate lookup keyed by exact composition; falls back to longest matching
// prefix in TerminalDemo.lookupCandidates().
export const CANDIDATE_TABLE: Record<string, Candidate[]> = {
  n: [
    { key: '1', text: '你' },
    { key: '2', text: '那' },
    { key: '3', text: '呢' },
    { key: '4', text: '能' },
    { key: '5', text: '年' },
  ],
  ni: [
    { key: '1', text: '你' },
    { key: '2', text: '妳' },
    { key: '3', text: '尼' },
    { key: '4', text: '泥' },
    { key: '5', text: '呢' },
  ],
  nihao: [
    { key: '1', text: '你好' },
    { key: '2', text: '妳好' },
    { key: '3', text: '尼豪' },
  ],
  s: [
    { key: '1', text: '是' },
    { key: '2', text: '说' },
    { key: '3', text: '上' },
    { key: '4', text: '时' },
    { key: '5', text: '谁' },
  ],
  shi: [
    { key: '1', text: '是' },
    { key: '2', text: '时' },
    { key: '3', text: '事' },
    { key: '4', text: '使' },
    { key: '5', text: '市' },
  ],
  shijie: [
    { key: '1', text: '世界' },
    { key: '2', text: '师姐' },
    { key: '3', text: '时节' },
  ],
};

// The scripted auto-play sequence. Reuses the README / MONKEY_TESTING strings.
export const DEMO_SCRIPT: ScriptStep[] = [
  { kind: 'toggle' },
  { kind: 'type', chars: 'ni' },
  { kind: 'wait', ms: 350 },
  { kind: 'type', chars: 'hao' },
  { kind: 'wait', ms: 600 },
  { kind: 'select', index: 0 }, // 你好
  { kind: 'type', chars: ' ' },
  { kind: 'type', chars: 'shi' },
  { kind: 'wait', ms: 300 },
  { kind: 'type', chars: 'jie' },
  { kind: 'select', index: 0 }, // 世界
  { kind: 'toggle' },
];

/** Find candidates for a composition: exact match, else longest matching prefix. */
export function lookupCandidates(composition: string): Candidate[] {
  if (!composition) return [];
  if (CANDIDATE_TABLE[composition]) return CANDIDATE_TABLE[composition];
  // longest prefix
  for (let i = composition.length - 1; i > 0; i--) {
    const prefix = composition.slice(0, i);
    if (CANDIDATE_TABLE[prefix]) return CANDIDATE_TABLE[prefix];
  }
  return [];
}

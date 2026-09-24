#!/usr/bin/env node
/**
 * 站点字体子集工具。
 *
 * 站点用 Sarasa Term SC Nerd（上游 28 MB/字重，自带 CJK + Nerd Font 图标）。
 * 仓库只存按站点实际用字子集化后的 woff2（三个字重各约 100 KB）与字符集清单
 * src/assets/fonts/subsetchars.txt。
 *
 *   node scripts/font-subset.mjs check            校验清单是否覆盖站点实际渲染文本
 *   node scripts/font-subset.mjs write --src DIR  重新生成清单与 woff2
 *
 * check 在改完文案后运行：有输出说明引入了清单外的新字，必须补进清单再重新子集化，
 * 否则该字会静默落到 fallback 字体，破坏全站字形统一。
 *
 * write 需要上游 TTF（sarasa-term-sc-{regular,semibold,bold}-nerd-font.ttf）与
 * `pip install fonttools brotli`。子集参数经字节级验证，可用同一份清单复现仓库
 * 中的 woff2。
 *
 * 字符集来源 = 构建产物的渲染 HTML 文本（SSR，含主题字符串）∪ 首页 tsx 源码里的
 * 动态字符串（演示动画候选词在 JS 运行后才出现，SSR HTML 抓不到）∪ og-image.html。
 * 不扫打包后的 js：那里混了 Prism 语言表（含西里尔、CJK 别名）等从不渲染的
 * 运行时数据，会误收噪声。
 */
import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';

const ROOT = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const FONT_DIR = path.join(ROOT, 'src', 'assets', 'fonts');
const CHARSET = path.join(FONT_DIR, 'subsetchars.txt');
const BUILD = path.join(ROOT, 'build');

const WEIGHTS = {'400': 'regular', '600': 'semibold', '700': 'bold'};

const ALLOWED = [
  [0x0000, 0x007f], [0x00a0, 0x00ff], [0x2000, 0x206f], [0x2070, 0x209f],
  [0x20a0, 0x20cf], [0x2100, 0x214f], [0x2150, 0x218f], [0x2190, 0x21ff],
  [0x2200, 0x22ff], [0x2460, 0x24ff], [0x2500, 0x257f], [0x2580, 0x259f],
  [0x25a0, 0x25ff], [0x2600, 0x26ff], [0x2700, 0x27bf], [0x3000, 0x303f],
  [0x3040, 0x30ff], [0x4e00, 0x9fff], [0xe000, 0xf8ff], [0xf900, 0xfaff],
  [0xfe30, 0xfe4f], [0xff00, 0xffef],
];

const allowed = (c) => {
  const o = c.codePointAt(0);
  if (o >= 0xd800 && o <= 0xdfff) return false;
  return ALLOWED.some(([lo, hi]) => o >= lo && o <= hi);
};

const add = (s, set) => {
  for (const c of s) if (c.trim() !== '' && allowed(c)) set.add(c);
};

const read = (f) => { try { return fs.readFileSync(f, 'utf8'); } catch { return ''; } };

const htmlText = (s) =>
  s.replace(/<(script|style)[^>]*>[\s\S]*?<\/\1>/gi, '').replace(/<[^>]+>/g, ' ');

/** 收集站点实际会渲染的字符。 */
const collect = () => {
  if (!fs.existsSync(BUILD)) {
    console.error(`缺少构建产物 ${BUILD}，请先 npm run build`);
    process.exit(1);
  }
  const chars = new Set();
  const walk = (dir) => {
    for (const e of fs.readdirSync(dir, {withFileTypes: true})) {
      const p = path.join(dir, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.name.endsWith('.html')) add(htmlText(read(p)), chars);
    }
  };
  walk(BUILD);
  // 首页 tsx 里有演示动画的候选词字面量，SSR HTML 抓不到
  add(read(path.join(ROOT, 'src/pages/index.tsx')), chars);
  add(htmlText(read(path.join(ROOT, 'og-image.html'))), chars);
  return chars;
};

const loadCharset = () => new Set(fs.readFileSync(CHARSET, 'utf8'));

const readArg = (name, argv) => {
  const i = argv.indexOf(name);
  return i === -1 ? undefined : argv[i + 1];
};

const cmdCheck = () => {
  const have = loadCharset();
  const seen = collect();
  const missing = [...seen].filter((c) => !have.has(c)).sort();
  console.log(`清单 ${have.size} 字；站点渲染 ${seen.size} 字`);
  if (missing.length) {
    console.log(`缺字（补进 ${path.relative(ROOT, CHARSET)} 后重新子集化）：`);
    console.log(missing.join(''));
    process.exit(1);
  }
  console.log('OK 无缺字');
};

const cmdWrite = (argv) => {
  const src = readArg('--src', argv);
  if (!src) {
    console.error('write 需要 --src DIR 指定上游 TTF 所在目录');
    process.exit(1);
  }

  const chars = collect();
  for (const c of loadCharset()) chars.add(c);  // 并入现有清单（保留安全余量）
  const text = [...chars].sort().join('');
  fs.writeFileSync(CHARSET, text, 'utf8');
  console.log(`写入 ${path.relative(ROOT, CHARSET)}（${chars.size} 字）`);

  for (const [num, name] of Object.entries(WEIGHTS)) {
    const ttf = path.join(src, `sarasa-term-sc-${name}-nerd-font.ttf`);
    if (!fs.existsSync(ttf)) {
      console.error(`缺少上游字体 ${ttf}`);
      process.exit(1);
    }
    const out = path.join(FONT_DIR, `stsn-${num}.woff2`);
    execFileSync('pyftsubset', [ttf,
      `--text-file=${CHARSET}`,
      '--flavor=woff2',
      '--no-hinting',
      '--layout-features=*',
      `--output-file=${out}`], {stdio: ['ignore', 'ignore', 'inherit']});
    console.log(`生成 ${path.relative(ROOT, out)}（${fs.statSync(out).size} 字节）`);
  }
};

const [cmd, ...rest] = process.argv.slice(2);
if (cmd === 'check') cmdCheck();
else if (cmd === 'write') cmdWrite(rest);
else {
  console.error('用法：font-subset.mjs check | write --src DIR');
  process.exit(1);
}

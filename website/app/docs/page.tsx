import type { Metadata } from 'next';

export const metadata: Metadata = {
  title: '文档 — term-ime',
  description: 'term-ime 配置、架构与使用文档',
};

const SECTIONS = [
  {
    id: 'config',
    title: '配置文件',
    body: `配置文件位于 ~/.config/term-ime/config.json(或 $XDG_CONFIG_HOME/term-ime/config.json)。
命令行可传路径覆盖:./build/term-ime /path/to/config.json

主要字段:
  - languages: 语言列表(id/name/schema/enabled),配置驱动,新增语言无需改代码
  - active_language: 启动时激活的语言 id
  - ui_language: 界面语言 (en / zh-CN / zh-TW / ja)
  - llama_ranker: 可选 LLM 候选词排序 (enabled / model_path / n_threads / backend)
  - log_level: debug / info / warn / error

日志写入 ~/.cache/term-ime/term-ime.log(HOME 未设时回落 /tmp)。`,
  },
  {
    id: 'shortcuts',
    title: '快捷键',
    body: `Ctrl+A 是前缀键,按下后下一个字节决定动作:

  Ctrl+A  Space    切换中英文模式
  Ctrl+A  A        切换 AI 候选词排序
  Ctrl+A  S        打开设置面板
  Ctrl+A  Ctrl+A   向 shell 发送字面 0x01
  Ctrl+A  <其他>   向 shell 转发 Ctrl+A + 该键

输入法候选状态:
  1-9    选择候选词
  Space  选择首个候选词
  Esc    取消输入
  Backspace  取消输入

设置面板:
  ↑/↓ 或 j/k  导航
  ←/→ 或 Enter 切换值
  Tab / Esc    关闭面板`,
  },
  {
    id: 'architecture',
    title: '架构',
    body: `四个 CMake 静态库组合,main.cpp 是薄封装:

  term-core      EventLoop(libuv)、App、InputProcessor(Boost.SML 状态机)、Config
  term-terminal  Pty、Screen、Parser(VT100/CSI)、ui/ 渲染层
  term-ime-lib   ImeEngine 抽象 + RimeIme、LanguageManager、CandidateRanker + LlamaRanker、KaomojiLib、ModelDownloader
  term-ime       可执行文件

关键流程:
  - 输入经 InputProcessor 分类:转发 shell / Ctrl+A 组合 / IME 组合
  - 中文模式小写字母触发 librime 组合,选中候选以 UTF-8 写入 PTY
  - LlamaRanker 在 worker 线程异步排序,版本号校验丢弃过期结果
  - 设置面板可见时 PTY 输出被抑制(shell 继续运行但不显示)`,
  },
  {
    id: 'build',
    title: '构建选项',
    body: `CMake 加速后端(默认 OFF):
  -DLLAMA_USE_CUDA=ON     CUDA
  -DLLAMA_USE_VULKAN=ON   Vulkan
  -DLLAMA_USE_METAL=ON    Metal/MPS (Mac)
  -DLLAMA_USE_NPU=ON      NPU (via Vulkan)

增量构建(避免 make build 的全量 clean):
  cmake --build build -j$(nproc)

子模块(必须,从源码构建):
  FTXUI spdlog nlohmann_json googletest librime llama.cpp sml utf8proc
  git submodule update --init --recursive

系统依赖:libuv1-dev、curl(librime 从 deps/librime 子模块构建)。`,
  },
  {
    id: 'testing',
    title: '测试',
    body: `四个测试可执行文件:
  term-ime-tests   UTF-8 / config / IME 状态 (gtest, 注册到 ctest)
  test-ui-jsx      UI JSX 组件 (plain main)
  test-settings    设置面板 (plain main)
  test-input-e2e   输入处理器状态机 (assert)

  make test                              # 跑 gtest
  cd build && ctest --output-on-failure  # ctest
  ./build/test-input-e2e                 # 单独跑

tests/*.py 和 test/test_all.sh 是独立 PTY 脚本,未接入 make/ctest。`,
  },
];

export default function DocsPage() {
  return (
    <div className="container docs-page">
      <nav className="docs-toc">
        <h3>文档</h3>
        <ul>
          {SECTIONS.map((s) => (
            <li key={s.id}>
              <a href={`#${s.id}`}>{s.title}</a>
            </li>
          ))}
        </ul>
        <p>
          <a href="/">← 返回首页</a>
        </p>
      </nav>

      <article className="docs-content">
        {SECTIONS.map((s) => (
          <section key={s.id} id={s.id} className="doc-section">
            <h2>{s.title}</h2>
            <pre>{s.body}</pre>
          </section>
        ))}
      </article>

      <style>{`
        .docs-page { display: grid; grid-template-columns: 200px 1fr; gap: 32px; padding: 32px 20px; }
        @media (max-width: 720px) { .docs-page { grid-template-columns: 1fr; } }
        .docs-toc h3 { color: var(--accent); font-size: 16px; margin-bottom: 12px; }
        .docs-toc h3::before { content: '# '; color: var(--text-faint); }
        .docs-toc ul { list-style: none; }
        .docs-toc li { margin: 6px 0; }
        .docs-toc a { color: var(--text-dim); font-size: 13px; }
        .docs-toc a:hover { color: var(--accent); }
        .docs-toc p { margin-top: 16px; }
        .doc-section { margin-bottom: 40px; }
        .doc-section h2 { color: var(--accent); font-size: 18px; margin-bottom: 12px; }
        .doc-section h2::before { content: '# '; color: var(--text-faint); }
        .doc-section pre {
          background: var(--bg-panel);
          border: 1px solid var(--border);
          border-radius: var(--radius);
          padding: 14px 16px;
          color: var(--text);
          font-size: 13px;
          line-height: 1.6;
          white-space: pre-wrap;
          word-break: break-word;
          overflow-x: auto;
        }
      `}</style>
    </div>
  );
}

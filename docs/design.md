# TTY 中文输入虚拟终端 - 模块设计

## 目录结构

```
term-ime/
├── src/
│   ├── main.cpp              # 程序入口
│   ├── terminal/
│   │   ├── pty.hpp           # PTY 管理
│   │   ├── pty.cpp
│   │   ├── screen.hpp        # 屏幕缓冲
│   │   ├── screen.cpp
│   │   ├── parser.hpp        # 转义序列解析
│   │   └── parser.cpp
│   ├── ime/
│   │   ├── engine.hpp        # 输入法引擎接口
│   │   ├── engine.cpp
│   │   ├── pinyin.hpp        # 拼音输入法
│   │   ├── pinyin.cpp
│   │   ├── dict.hpp          # 词库加载
│   │   └── dict.cpp
│   ├── ui/
│   │   ├── renderer.hpp      # TTY 渲染
│   │   ├── renderer.cpp
│   │   └── candidate_bar.hpp # 候选栏
│   └── util/
│       ├── utf8.hpp          # UTF-8 工具
│       └── utf8.cpp
├── data/
│   └── pinyin.dict           # 拼音词库
├── Makefile
└── README.md
```

## 模块接口定义

### 1. PTY 模块 (`terminal/pty.hpp`)

```cpp
#pragma once
#include <string>
#include <vector>
#include <optional>

class Pty {
public:
    Pty();
    ~Pty();

    // 创建 PTY 并启动子进程
    bool spawn(const std::string& shell = "/bin/bash");

    // 读取子进程输出
    std::optional<std::vector<uint8_t>> read();

    // 写入子进程输入
    bool write(const std::vector<uint8_t>& data);

    // 获取文件描述符（用于 poll）
    int fd() const;

    // 设置窗口大小
    void resize(int rows, int cols);

private:
    int master_fd_ = -1;
    int pid_ = -1;
};
```

### 2. 屏幕缓冲模块 (`terminal/screen.hpp`)

```cpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct Cell {
    char32_t ch = U' ';      // UTF-32 字符
    uint8_t fg = 7;          // 前景色 (0-15)
    uint8_t bg = 0;          // 背景色 (0-15)
    bool wide = false;       // 是否宽字符 (CJK)
};

class Screen {
public:
    Screen(int rows, int cols);

    // 字符操作
    void put(char32_t ch, int row, int col);
    Cell get(int row, int col) const;

    // 光标控制
    void move_cursor(int row, int col);
    int cursor_row() const;
    int cursor_col() const;

    // 滚动
    void scroll_up(int n = 1);

    // 清除
    void clear();
    void clear_line();

    // 尺寸
    int rows() const;
    int cols() const;
    void resize(int rows, int cols);

private:
    std::vector<std::vector<Cell>> grid_;
    int cursor_row_ = 0;
    int cursor_col_ = 0;
};
```

### 3. 转义序列解析模块 (`terminal/parser.hpp`)

```cpp
#pragma once
#include "screen.hpp"
#include <cstdint>

class Parser {
public:
    Parser(Screen& screen);

    // 输入字节流，更新屏幕状态
    void feed(const uint8_t* data, size_t len);

private:
    Screen& screen_;

    // 解析状态
    enum class State { Normal, Escape, CSI };
    State state_ = State::Normal;

    // CSI 参数缓冲
    std::string csi_params_;
};
```

### 4. 输入法引擎接口 (`ime/engine.hpp`)

```cpp
#pragma once
#include <string>
#include <vector>
#include <functional>

// 输入法状态
enum class ImeState {
    Inactive,    // 未激活
    Composing,   // 正在输入拼音
    Selecting    // 正在选词
};

// 候选词
struct Candidate {
    std::u32string text;    // 候选文本
    std::string code;       // 拼音编码
};

class ImeEngine {
public:
    virtual ~ImeEngine() = default;

    // 输入字符，返回是否被 IME 消费
    virtual bool input(char ch) = 0;

    // 获取当前状态
    virtual ImeState state() const = 0;

    // 获取当前拼音缓冲
    virtual std::string buffer() const = 0;

    // 获取候选词列表
    virtual std::vector<Candidate> candidates() const = 0;

    // 选择候选词（索引），返回上屏文本
    virtual std::u32string select(int index) = 0;

    // 取消输入
    virtual void cancel() = 0;

    // 翻页
    virtual void page_up() = 0;
    virtual void page_down() = 0;
};
```

### 5. 拼音输入法 (`ime/pinyin.hpp`)

```cpp
#pragma once
#include "engine.hpp"
#include "dict.hpp"
#include <memory>

class PinyinIme : public ImeEngine {
public:
    PinyinIme(std::unique_ptr<Dict> dict);

    bool input(char ch) override;
    ImeState state() const override;
    std::string buffer() const override;
    std::vector<Candidate> candidates() const override;
    std::u32string select(int index) override;
    void cancel() override;
    void page_up() override;
    void page_down() override;

private:
    std::unique_ptr<Dict> dict_;
    std::string buffer_;
    std::vector<Candidate> candidates_;
    size_t page_start_ = 0;
    size_t page_size_ = 5;
    ImeState state_ = ImeState::Inactive;
};
```

### 6. 词库模块 (`ime/dict.hpp`)

```cpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

class Dict {
public:
    // 从文件加载词库
    bool load(const std::string& path);

    // 根据拼音查询候选词
    std::vector<std::u32string> query(const std::string& pinyin) const;

private:
    // pinyin -> [candidates...]
    std::unordered_map<std::string, std::vector<std::u32string>> entries_;
};
```

### 7. 渲染模块 (`ui/renderer.hpp`)

```cpp
#pragma once
#include "../terminal/screen.hpp"
#include "../ime/engine.hpp"
#include <string>

class Renderer {
public:
    Renderer();

    // 初始化 TTY（设置 raw 模式等）
    void init();

    // 恢复 TTY 状态
    void restore();

    // 渲染屏幕内容
    void render(const Screen& screen);

    // 渲染候选栏（屏幕底部）
    void render_candidates(const std::vector<Candidate>& candidates,
                           size_t selected, const std::string& buffer);

    // 读取用户输入
    int read_key();

private:
    int tty_fd_ = -1;
    std::string saved_termios_;
};
```

### 8. 主循环 (`main.cpp` 逻辑)

```cpp
int main() {
    // 1. 初始化
    Renderer renderer;
    renderer.init();

    Pty pty;
    pty.spawn("/bin/bash");

    Screen screen(24, 80);
    Parser parser(screen);

    auto dict = std::make_unique<Dict>();
    dict->load("data/pinyin.dict");

    PinyinIme ime(std::move(dict));

    // 2. 主循环
    while (running) {
        // poll PTY 输出 + 用户输入
        // PTY 输出 -> parser -> screen
        // 用户输入 -> ime 或 pty
        // 渲染 screen + 候选栏
    }

    // 3. 清理
    renderer.restore();
    return 0;
}
```

## 词库格式 (`data/pinyin.dict`)

```
# 格式: 拼音<TAB>候选词
ni      你
ni      妳
hao     好
nihao   你好
zhongguo 中国
```

## 下一步

1. 实现最小 PTY + 屏幕渲染（无 IME）
2. 验证基本终端功能
3. 加入 IME 模块
4. 集成测试

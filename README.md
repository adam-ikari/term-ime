# TTY 中文输入虚拟终端

在 Linux TTY 环境中运行的虚拟终端，内置拼音输入法。

## 构建

```bash
mkdir -p build && cd build
cmake ..
make
```

## 运行

```bash
./term-ime
```

**注意**: 需要在真实 TTY 或支持 alternate screen 的终端中运行。

## 使用方法

- 输入小写字母开始拼音输入
- 数字键 1-9 选择候选词
- Escape 取消输入
- Ctrl+C 退出程序

## 词库

词库文件位于 `data/pinyin.dict`，格式：

```
拼音<TAB>汉字
```

## 功能

- [x] PTY 创建与子进程管理
- [x] 屏幕缓冲
- [x] VT100 转义序列解析
- [x] UTF-8 输入输出
- [x] 拼音输入法
- [x] 终端大小变化处理
- [ ] 更多转义序列支持
- [ ] 颜色支持

# Dora C/C++ 节点创建教程 

在 Dora 框架下使用 C 语言编写、构建和运行节点。本示例包含两个节点：

- **Node A**: 每秒发送2次字符串 "Hello from Node A (C API)!"。
- **Node B**: 接收 Node A 发送的字符串，并打印出该字符串及当前的接收时间。

## 文件结构
- `node_a.c`: Node A 的源代码 (使用 C API)。
- `node_b.c`: Node B 的源代码 (使用 C API)。
- `CMakeLists.txt`: 构建配置文件，直接指定 Dora SDK 路径。
- `dataflow.yml`: Dora 数据流图定义。

## 构建步骤
1. **确认 Dora 路径**：
   打开 `CMakeLists.txt`，确保 `DORA_ROOT` 路径与您的系统环境一致。默认设置为：
   `$ENV{HOME}/dora`

2. **编译**：
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

## 数据流图配置 (dataflow.yml)
在 `dora_node_create` 目录下创建一个 `dataflow.yml` 文件：

```yaml
nodes:
  - id: node_a
    custom:
      source: build/node_a
    outputs:
      - data
  - id: node_b
    custom:
      source: build/node_b
    inputs:
      node_a/data: data
```

## 运行
启动 Dora 数据流：
```bash
dora start dataflow.yml
```

运行后，Node B 将在终端输出如下格式的信息：
```
[Node B] Received: Hello from Node A (C API)! | Time: Tue Apr 14 10:00:00 2026
```

# 模型目录说明

```
models/
├── checkpoints/          PyTorch 训练 checkpoint (.pth)
│   ├── unmark/           无球接应模型
│   └── shot_target/      射门目标点模型
├── exported/             CppDNN 格式权重 (.txt) + 元信息 (.json)
└── cpp_ready/            准备集成到 C++ 的最终版本
```

## 使用方式

### 训练并导出

```bash
python3 ml/trainers/train_unmark.py \
    --data data/extracted/unmark/ \
    --output models/checkpoints/unmark/ \
    --export models/exported/
```

### 部署到 C++

将 `models/exported/*.txt` 复制到 `bolt_submission/` 或 `build/bin/`，
C++ 代码通过 `CppDNN::DeepNueralNetwork` 加载。

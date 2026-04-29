# 数据资产清单

## 📊 数据统计总览

### 原始日志文件
- **总数**: 64个文件
  - 官方比赛日志: 50个 (.rcg.gz + .rcl.gz，位于logs/目录)
  - 本地测试日志: 14个 (.rcg，位于项目根目录)
- **总大小**: 987MB
- **存储位置**: 保持原位置不移动

### 已提取特征数据集
| 数据集 | 样本数 | 特征维度 | 存储路径 | 状态 |
|--------|--------|----------|----------|------|
| shot_target_v1 | 1,080 | 27 | extracted/shot_target/ | ✅ 已废弃 |
| shot_target_v2 | 9,096 | 27 | extracted/shot_target_v2/ | ✅ 生产使用 |
| unmark_v1 | 2,500 | 30 | extracted/unmark/ | ✅ 已废弃 |
| unmark_v2 | 116,437 | 30 | extracted/unmark_v2/ | ✅ 生产使用 |
| pass_decision | - | 35 | extracted/pass_decision/ | 🔄 待创建 |

## 📁 目录结构

```
data/
├── README.md                    # 目录使用说明
├── DATA_INVENTORY.md           # 本文件：数据资产清单
├── raw_logs/                   # 原始日志索引（文件保持在原位置）
│   ├── README.md
│   ├── official/               # 官方日志索引 (../../logs/*.rcg.gz)
│   └── local/                  # 本地日志索引 (../../*.rcg)
├── extracted/                  # 提取的特征数据
│   ├── shot_target/           # 射门v1（已废弃）
│   ├── shot_target_v2/        # 射门v2（生产）
│   ├── unmark/                # 跑位v1（已废弃）
│   ├── unmark_v2/             # 跑位v2（生产）
│   └── pass_decision/         # 传球（待创建）
├── datasets/                   # 训练/测试分割数据
│   └── shot_target_v1_test.csv
└── manifests/                  # 日志文件清单和元数据
    ├── manifest_v1.json
    ├── manifest_v2.json
    └── raw_scan.jsonl
```

## 🔧 数据提取工具

### 已有提取器
| 提取器 | 输入 | 输出 | 特征维度 |
|--------|------|------|----------|
| extract_shot_target_dataset.py | .rcg日志 | shot_target_v2/ | 27维 |
| extract_unmark_dataset.py | .rcg日志 | unmark_v2/ | 30维 |

### 待开发提取器
- [ ] extract_pass_dataset.py - 传球决策数据提取（35维特征）

## 🎯 下一步工作

### 任务#4: 收集传球训练数据
1. 创建 `ml/extractors/extract_pass_dataset.py`
2. 参考 `extract_shot_target_dataset.py` 和 `extract_unmark_dataset.py`
3. 提取35维传球特征：
   - 传球者状态 (6维)
   - 接球者状态 (8维)
   - 球状态 (4维)
   - 传球参数 (3维)
   - 对手威胁 (8维)
   - 战术价值 (6维)
4. 目标：从64个日志文件提取10,000+传球样本
5. 输出到 `data/extracted/pass_decision/`

### 任务#5: 训练传球决策MLP模型
1. 创建 `ml/train_pass_decision.py`
2. 网络结构：35 → 64 → 32 → 16 → 1
3. 输出权重文件到 `build_v4/bin/pass_decision_mlp_weights.txt`

## 📝 使用示例

### 扫描所有日志文件
```python
import glob

# 官方日志
official_logs = glob.glob("logs/*.rcg.gz")
print(f"官方日志: {len(official_logs)}个")

# 本地日志
local_logs = glob.glob("*.rcg")
print(f"本地日志: {len(local_logs)}个")

# 合并
all_logs = official_logs + local_logs
print(f"总计: {len(all_logs)}个日志文件")
```

### 加载已提取数据
```python
import numpy as np

# 加载射门数据
shot_features = np.load("data/extracted/shot_target_v2/features.npy")
shot_labels = np.load("data/extracted/shot_target_v2/labels.npy")
print(f"射门样本: {len(shot_features)}")

# 加载跑位数据
unmark_features = np.load("data/extracted/unmark_v2/features.npy")
unmark_labels = np.load("data/extracted/unmark_v2/labels.npy")
print(f"跑位样本: {len(unmark_features)}")
```

## 🗂️ 数据版本管理

### 版本命名规则
- v1: 初始版本（小规模测试）
- v2: 生产版本（大规模提取，优化特征）
- v3+: 未来迭代版本

### 当前生产版本
- shot_target: **v2** (9,096样本)
- unmark: **v2** (116,437样本)
- pass_decision: **待创建**

---
*最后更新: 2026-04-25*

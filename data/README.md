# Bolt ML Training Data

本目录包含用于训练Bolt机器学习模型的完整数据集和官方比赛日志。

## 目录结构

```
data/
├── extracted/          # 已提取的训练数据集（.npy格式）
│   ├── shot_target_v2/      # 射门目标数据集 (9,096样本)
│   ├── unmark_v2/           # 跑位接球数据集 (116,437样本)
│   ├── pass_decision_v1/    # 传球决策数据集 (35,051样本)
│   └── defense_positioning_v2/ # 防守站位数据集 (6,400样本)
├── raw_logs/           # 官方比赛日志（.rcg.gz格式）
│   └── *.rcg.gz             # 33个官方比赛日志 + 本地测试日志
├── manifests/          # 数据集清单文件
│   ├── manifest_v1.json     # 数据集v1清单
│   └── manifest_v2.json     # 数据集v2清单
└── logs/               # 本地测试日志

总大小: ~1.6GB
```

## 数据集详情

### 1. 射门目标数据集 (shot_target_v2)
- **样本数**: 9,096
- **特征维度**: 27维
- **标签**: 
  - `labels_goal.npy`: 是否进球 (0/1)
  - `labels_on_target.npy`: 是否射正 (0/1)
  - `labels_shot_value.npy`: 射门价值评分
- **用途**: 训练射门目标选择模型

### 2. 跑位接球数据集 (unmark_v2)
- **样本数**: 116,437
- **特征维度**: 30维
- **标签**:
  - `labels_receivable.npy`: 是否成功接球 (0/1)
  - `labels_fv10.npy`: 接球后10周期场地价值
  - `labels_fv20.npy`: 接球后20周期场地价值
- **用途**: 训练跑位接球模型

### 3. 传球决策数据集 (pass_decision_v1)
- **样本数**: 35,051
- **特征维度**: 35维
- **标签**:
  - `labels_success.npy`: 传球是否成功 (0/1)
  - `labels_fv10.npy`: 传球后10周期场地价值
  - `labels_fv20.npy`: 传球后20周期场地价值
- **用途**: 训练传球决策模型

### 4. 防守站位数据集 (defense_positioning_v2)
- **样本数**: 6,400 ⚠️ **数据量不足，需扩充**
- **特征维度**: 32维
- **标签**:
  - `labels_success.npy`: 防守是否成功 (0/1)
- **用途**: 训练防守站位模型
- **待优化**: 需要从raw_logs提取更多样本（目标20,000+）

## 官方比赛日志

`raw_logs/` 目录包含33个官方比赛的压缩日志文件：
- 格式: `.rcg.gz` (比赛记录) + `.rcl.gz` (日志)
- 来源: RoboCup 2023官方比赛
- 总大小: ~1.3GB (压缩后)
- 用途: 提取更多训练数据，扩充数据集

## 使用方法

### 1. 训练现有模型

使用已提取的数据集直接训练：

```bash
# 训练射门模型
python3 ml/train_shot_mlp.py \
    --data data/extracted/shot_target_v2/ \
    --output models/shot_target_mlp_best.pth

# 训练传球模型
python3 ml/train_pass_mlp.py \
    --data data/extracted/pass_decision_v1/ \
    --output models/pass_decision_mlp_best.pth

# 训练防守模型
python3 ml/train_defense_mlp.py \
    --data data/extracted/defense_positioning_v2/ \
    --output models/defense_positioning_mlp_best.pth
```

### 2. 从官方日志提取新数据

扩充防守数据集（推荐）：

```bash
# 从官方日志提取防守数据
python3 ml/extractors/extract_defense_dataset.py \
    --manifest data/manifests/manifest_v2.json \
    --output data/extracted/defense_positioning_v3/ \
    --max-files 33

# 预期输出: 20,000+防守样本
```

提取其他类型数据：

```bash
# 提取射门数据
python3 ml/extractors/extract_shot_target_dataset.py \
    --manifest data/manifests/manifest_v2.json \
    --output data/extracted/shot_target_v3/

# 提取跑位数据
python3 ml/extractors/extract_unmark_dataset.py \
    --manifest data/manifests/manifest_v2.json \
    --output data/extracted/unmark_v3/

# 提取传球数据
python3 ml/extractors/extract_pass_dataset.py \
    --manifest data/manifests/manifest_v2.json \
    --output data/extracted/pass_decision_v2/
```

### 3. 转换权重为C++格式

训练完成后，需要将PyTorch权重转换为C++可读格式：

```bash
# 转换射门模型权重
python3 ml/convert_shot_weights_to_cppdnn.py \
    --input models/shot_target_mlp_best.pth \
    --output shot_target_mlp_weights.txt

# 转换传球模型权重
python3 ml/convert_pass_weights_to_cppdnn.py \
    --input models/pass_decision_mlp_best.pth \
    --output pass_decision_mlp_weights.txt

# 转换防守模型权重
python3 ml/convert_defense_weights_to_cppdnn.py \
    --input models/defense_positioning_mlp_best.pth \
    --output defense_positioning_mlp_weights.txt

# 复制权重文件到运行目录
cp *_weights.txt build/bin/
```

## 数据格式说明

### 特征文件 (features.npy)
- 格式: NumPy数组，shape=(N, D)
- N: 样本数量
- D: 特征维度
- 数据类型: float32

### 标签文件 (labels_*.npy)
- 格式: NumPy数组，shape=(N,)
- N: 样本数量（与features对应）
- 数据类型: float32

### 元数据文件 (meta.jsonl)
- 格式: JSON Lines（每行一个JSON对象）
- 内容: 每个样本的上下文信息（时间、位置、比赛状态等）
- 用途: 调试和数据分析

## 优先优化建议

根据 `/home/yucc/ML优化待完成计划.md`，当前最高优先级任务：

🔥 **防守站位数据集扩充与模型重训练**
- 当前问题: 防守模型仅6,400样本，效果不佳
- 解决方案: 从raw_logs的33个官方日志提取20,000+样本
- 预期效果: 防守能力提升15-20%，失球数减少10-15%

详细步骤参见: `/home/yucc/ML优化待完成计划.md` 第A项

## 依赖工具

- **RCG解析器**: `tools/rcg/parse_rcg.py`
- **数据提取器**: `ml/extractors/extract_*_dataset.py`
- **训练脚本**: `ml/train_*_mlp.py`
- **权重转换**: `ml/convert_*_weights_to_cppdnn.py`

## 远程训练环境

如需GPU训练，使用远程服务器：

```bash
# SSH连接
ssh -p 53718 -i /mnt/hgfs/robocup/id_rsa zhengli@172.28.6.47

# 注意: 文件必须放在 /mnt/second/zhengli/ 路径范围内
```

## 数据来源

- **官方日志**: RoboCup 2023官方比赛记录
- **本地日志**: Bolt vs Cyrus2DBase/BoltBase测试对战
- **提取时间**: 2026-04-24 ~ 2026-04-28
- **数据版本**: v2 (最新)

## 更新日志

### 2026-04-29
- ✅ 完整迁移robocup-Bolt-onlyml数据到robocup-Bolt
- ✅ 迁移33个官方比赛日志 (1.3GB)
- ✅ 迁移所有已提取数据集 (shot_target_v2, unmark_v2, pass_decision_v1, defense_positioning_v2)
- ✅ 迁移manifest文件和元数据
- 📊 总数据量: 1.6GB

---

**维护者**: Bolt开发团队  
**最后更新**: 2026-04-29

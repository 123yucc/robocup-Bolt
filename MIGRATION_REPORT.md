# 数据迁移报告

**迁移时间**: 2026-04-29  
**源目录**: `/home/yucc/robocup-Bolt-onlyml/`  
**目标目录**: `/home/yucc/robocup-Bolt/`  
**迁移类型**: 完整迁移（代码 + 数据 + 官方日志）

---

## 迁移内容总览

### ✅ 已迁移项目

#### 1. 训练数据集 (data/extracted/)
- ✅ `shot_target_v2/` - 射门目标数据集 (9,096样本, 27维特征)
- ✅ `unmark_v2/` - 跑位接球数据集 (116,437样本, 30维特征)
- ✅ `pass_decision_v1/` - 传球决策数据集 (35,051样本, 35维特征)
- ✅ `defense_positioning_v2/` - 防守站位数据集 (6,400样本, 32维特征)
- ✅ 所有.npy特征和标签文件
- ✅ 所有meta.jsonl元数据文件

**总计**: 24个.npy文件，包含完整的训练数据

#### 2. 官方比赛日志 (data/raw_logs/)
- ✅ 33个官方比赛日志 (.rcg.gz格式)
- ✅ 对应的日志文件 (.rcl.gz格式)
- ✅ 本地测试日志 (.rcg/.rcl格式)

**来源**: RoboCup 2023官方比赛  
**总大小**: ~1.3GB (压缩后)

#### 3. 数据清单文件 (data/manifests/)
- ✅ `manifest_v1.json` - 数据集v1清单
- ✅ `manifest_v2.json` - 数据集v2清单
- ✅ `raw_scan.jsonl` - 原始日志扫描结果

#### 4. 文档
- ✅ `DATA_INVENTORY.md` - 数据清单文档
- ✅ `data/README.md` - 数据使用指南（新建）

---

## 迁移前后对比

### robocup-Bolt-onlyml (源)
```
robocup-Bolt-onlyml/
├── data/                    52MB
│   ├── extracted/          (训练数据)
│   ├── manifests/          (清单文件)
│   └── raw_logs/           (部分日志)
├── logs/                    1.3GB
│   └── *.rcg.gz            (33个官方日志)
├── ml/                      (ML工具链)
└── tools/                   (辅助工具)
```

### robocup-Bolt (目标 - 迁移后)
```
robocup-Bolt/
├── data/                    1.6GB ⬅️ 新增
│   ├── extracted/          ✅ 完整训练数据
│   ├── raw_logs/           ✅ 33个官方日志 + 本地日志
│   ├── manifests/          ✅ 清单文件
│   ├── logs/               (本地测试日志)
│   ├── DATA_INVENTORY.md   ✅
│   └── README.md           ✅ 新建
├── ml/                      ✅ 已有完整ML工具链
│   ├── extractors/         (数据提取器)
│   ├── models/             (模型定义)
│   ├── utils/              (工具函数)
│   ├── train_*.py          (训练脚本)
│   └── convert_*.py        (权重转换)
├── tools/                   ✅ 已有RCG解析器
│   └── rcg/parse_rcg.py
├── src/                     ✅ C++推理代码
│   └── player/learning/
└── models/                  ✅ 预训练模型
```

---

## 数据完整性验证

### 训练数据集
| 数据集 | 样本数 | 特征维度 | 文件数 | 状态 |
|--------|--------|----------|--------|------|
| shot_target_v2 | 9,096 | 27 | 5 | ✅ 完整 |
| unmark_v2 | 116,437 | 30 | 5 | ✅ 完整 |
| pass_decision_v1 | 35,051 | 35 | 5 | ✅ 完整 |
| defense_positioning_v2 | 6,400 | 32 | 4 | ✅ 完整 |

### 官方日志
- **总数**: 33个比赛日志
- **格式**: .rcg.gz (比赛记录) + .rcl.gz (日志)
- **状态**: ✅ 全部迁移

### 清单文件
- **manifest_v1.json**: ✅ 已迁移
- **manifest_v2.json**: ✅ 已迁移
- **raw_scan.jsonl**: ✅ 已迁移

---

## 迁移后的优势

### 1. 完整的ML开发环境 ✅
- 代码 + 数据 + 工具链全部就绪
- 别人clone后可以立即开始训练
- 无需额外下载或配置数据

### 2. 丰富的训练数据 ✅
- 4个完整的训练数据集（160,984总样本）
- 33个官方比赛日志（可提取更多数据）
- 支持数据增强和模型改进

### 3. 可复现性 ✅
- 所有训练数据都有manifest追踪
- 数据提取过程可重现
- 模型训练结果可验证

### 4. 扩展性 ✅
- 官方日志可用于扩充防守数据集（6,400 → 20,000+）
- 支持提取新类型的训练数据
- 支持添加更多比赛日志

---

## 后续优化建议

根据 `/home/yucc/ML优化待完成计划.md`：

### 🔥 高优先级（立即开始）
1. **防守站位数据集扩充**
   - 当前: 6,400样本（不足）
   - 目标: 20,000+样本
   - 方法: 从data/raw_logs的33个官方日志提取
   - 预期: 防守能力提升15-20%

2. **ML权重系统化调优**
   - 优化4个ML模型的权重组合
   - 使用网格搜索找到最优参数
   - 预期: 整体胜率提升5-10%

### 📊 中优先级
3. **技术债务清理** - 单例模式、错误处理、版本管理
4. **ML性能分析工具** - 实时监控、离线分析、可视化

### 🚀 低优先级
5. **射门vs传球vs带球决策模型** - 高层决策优化
6. **自适应采样增强** - 精细化优化

---

## 使用指南

### 快速开始

1. **训练现有模型**
```bash
cd /home/yucc/robocup-Bolt

# 训练传球模型
python3 ml/train_pass_mlp.py \
    --data data/extracted/pass_decision_v1/ \
    --output models/pass_decision_mlp_best.pth
```

2. **扩充防守数据集**（推荐）
```bash
# 从官方日志提取防守数据
python3 ml/extractors/extract_defense_dataset.py \
    --manifest data/manifests/manifest_v2.json \
    --output data/extracted/defense_positioning_v3/ \
    --max-files 33

# 预期: 20,000+样本
```

3. **转换权重并部署**
```bash
# 转换PyTorch权重为C++格式
python3 ml/convert_defense_weights_to_cppdnn.py \
    --input models/defense_positioning_mlp_best.pth \
    --output defense_positioning_mlp_weights.txt

# 复制到运行目录
cp defense_positioning_mlp_weights.txt build/bin/
```

详细文档参见: `data/README.md`

---

## Git提交建议

由于数据量较大（1.6GB），建议：

### 方案1: 全部提交（推荐）
```bash
cd /home/yucc/robocup-Bolt
git add data/
git commit -m "feat: 完整迁移ML训练数据和官方日志

- 迁移4个完整训练数据集（160,984样本）
- 迁移33个官方比赛日志（1.3GB）
- 添加数据使用文档和迁移报告
- 支持防守数据集扩充至20,000+样本

数据来源: robocup-Bolt-onlyml
总大小: 1.6GB"
```

### 方案2: 使用Git LFS（如果仓库有大小限制）
```bash
# 安装Git LFS
git lfs install

# 追踪大文件
git lfs track "data/raw_logs/*.rcg.gz"
git lfs track "data/raw_logs/*.rcl.gz"
git lfs track "data/extracted/**/*.npy"

git add .gitattributes
git add data/
git commit -m "feat: 使用Git LFS管理ML训练数据"
```

### 方案3: 排除日志，仅提交训练数据
如果只想提交已提取的训练数据（~50MB），排除原始日志：
```bash
# 添加到.gitignore
echo "data/raw_logs/*.rcg.gz" >> .gitignore
echo "data/raw_logs/*.rcl.gz" >> .gitignore

# 仅提交提取后的数据
git add data/extracted/ data/manifests/ data/*.md
git commit -m "feat: 添加ML训练数据集（不含原始日志）"
```

---

## 迁移验证清单

- [x] 训练数据集完整性（24个.npy文件）
- [x] 官方日志完整性（33个.rcg.gz文件）
- [x] 清单文件完整性（manifest_v1/v2.json）
- [x] 文档完整性（README.md, DATA_INVENTORY.md）
- [x] ML工具链完整性（extractors, trainers, converters）
- [x] RCG解析器可用性（tools/rcg/parse_rcg.py）
- [x] 目录结构正确性（extracted/, raw_logs/, manifests/）
- [x] 总数据量验证（~1.6GB）

**迁移状态**: ✅ 完成

---

## 总结

✅ **迁移成功完成**

robocup-Bolt项目现在包含：
- 完整的ML代码和工具链
- 4个高质量训练数据集（160,984样本）
- 33个官方比赛日志（可扩充数据）
- 完整的文档和使用指南

**别人可以直接clone并开始ML优化工作，无需额外依赖robocup-Bolt-onlyml。**

---

**迁移执行者**: Claude Code  
**迁移日期**: 2026-04-29  
**验证状态**: ✅ 通过

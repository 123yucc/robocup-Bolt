# ML权重调优工具

本目录包含用于系统化调优ML权重的自动化工具。

## 文件说明

- `tune_ml_weights.sh` - 自动化权重调优脚本
- `analyze_tuning_results.py` - 结果分析脚本

## 使用方法

### 1. 运行权重调优

```bash
cd /path/to/robocup-Bolt
./tools/tune_ml_weights.sh
```

该脚本会：
- 自动测试不同的权重组合
- 每个配置运行5场比赛
- 记录胜/平/负、进球数等统计数据
- 将结果保存到 `ml_tuning_results/tuning_results.csv`

### 2. 分析结果

```bash
python3 tools/analyze_tuning_results.py ml_tuning_results/tuning_results.csv
```

分析脚本会：
- 生成每个权重的性能曲线图
- 找出最佳权重配置
- 生成详细的调优报告
- 结果保存在 `ml_tuning_results/analysis/` 目录

### 3. 应用最佳权重

根据分析结果，手动更新代码中的权重值：

**射门权重** (`src/player/sample_field_evaluator.cpp`):
```cpp
static const double LAMBDA_SHOT = <最佳值>;
```

**跑位权重** (`src/player/bhv_unmark.cpp`):
```cpp
static const double LAMBDA_UNMARK = <最佳值>;
```

**传球权重** (`src/player/sample_field_evaluator.cpp`):
```cpp
static const double LAMBDA_PASS = <最佳值>;
```

**防守权重** (根据实际集成位置):
```cpp
static const double LAMBDA_DEFENSE = <最佳值>;
```

## 权重候选值

当前配置的候选值范围：

- `LAMBDA_SHOT`: 25000, 50000, 75000, 100000, 150000, 200000
- `LAMBDA_UNMARK`: 5.0, 10.0, 15.0, 20.0, 30.0
- `LAMBDA_PASS`: 25.0, 50.0, 75.0, 100.0
- `LAMBDA_DEFENSE`: 15.0, 30.0, 45.0, 60.0

可以根据需要修改 `tune_ml_weights.sh` 中的候选值。

## 调优策略

当前采用**单变量调优**策略：
- 每次只改变一个权重
- 其他权重保持默认值
- 这样可以独立评估每个权重的影响

如需进行**多变量联合优化**，可以修改脚本添加网格搜索或贝叶斯优化。

## 评分公式

综合得分计算公式：
```
score = 3 × wins + draws - 2 × losses + goal_diff
```

其中：
- `wins`: 胜场数
- `draws`: 平局数
- `losses`: 负场数
- `goal_diff`: 净胜球数

## 注意事项

1. **测试环境一致性**：确保每次测试使用相同的对手和环境配置
2. **样本数量**：默认每个配置测试5场，可根据需要调整 `NUM_GAMES_PER_CONFIG`
3. **编译时间**：每个配置都需要重新编译，整个调优过程可能需要较长时间
4. **结果可靠性**：建议在多个不同对手上验证最佳配置的性能

## 自定义调优

### 修改候选值范围

编辑 `tune_ml_weights.sh`：

```bash
LAMBDA_SHOT_VALUES=(自定义值列表)
LAMBDA_UNMARK_VALUES=(自定义值列表)
# ...
```

### 修改测试场次

编辑 `tune_ml_weights.sh`：

```bash
NUM_GAMES_PER_CONFIG=10  # 改为10场
```

### 添加新的权重参数

1. 在 `tune_ml_weights.sh` 中添加新的候选值数组
2. 在 `test_configuration()` 函数中添加权重更新逻辑
3. 在 `analyze_tuning_results.py` 中添加对应的分析逻辑

## 示例输出

```
=== 调优 LAMBDA_SHOT ===
测试配置: SHOT=25000, UNMARK=10.0, PASS=50.0, DEFENSE=30.0
结果: W=3 D=1 L=1 GF=8 GA=5 Score=11.00

测试配置: SHOT=50000, UNMARK=10.0, PASS=50.0, DEFENSE=30.0
结果: W=4 D=0 L=1 GF=10 GA=4 Score=16.00
...

总体最佳配置:
  LAMBDA_SHOT = 75000
  LAMBDA_UNMARK = 15.0
  LAMBDA_PASS = 75.0
  LAMBDA_DEFENSE = 30.0
  综合得分: 18.50
```

## 依赖

- bash
- Python 3.6+
- pandas
- matplotlib

安装Python依赖：
```bash
pip3 install pandas matplotlib
```

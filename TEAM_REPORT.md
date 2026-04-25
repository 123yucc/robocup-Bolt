# BoltV6_ML 球队描述报告

## 1. 球队概述

`BoltV6_ML` 是基于 `BoltV6` 分支改造的 RoboCup Soccer 2D 仿真队伍版本。该版本保留了 `BoltV6` 原有的整体战术框架、阵型配置、基础行为决策、传球与射门规划逻辑，并从 `ml` 分支导入训练好的机器学习权重，用于增强无球跑位和射门机会评估。

本队伍的目标不是完全替换原有启发式决策系统，而是在关键决策环节中加入轻量级神经网络评分，形成“传统规则 + 机器学习残差评分”的混合决策架构。

## 2. 项目来源与分支关系

- 基础代码来源：`BoltV6` 分支
- 机器学习模型来源：`ml` 分支
- 当前整合版本：`BoltV6_ML`
- 本地路径：`/home/ubuntu/robocup/BoltV6_ML`

`BoltV6_ML` 可以理解为一个面向比赛测试的集成版本：它继承 `BoltV6` 的主体比赛能力，同时导入 `ml` 分支导出的 CppDNN 格式模型权重，并将对应推理代码接入到 C++ 客户端中。

## 3. 整体技术架构

球队仍然采用 RoboCup 2D 常见的分层结构：

1. **Server 通信层**：通过 `librcsc` 与 `rcssserver` 通信，接收视觉、听觉、身体状态等信息。
2. **世界模型层**：维护球、队友、对手、比赛模式、体力等状态。
3. **基础行为层**：实现拦截、转身、移动、带球、防守、守门等原子行为。
4. **战术规划层**：生成传球、射门、盘带、清球等候选动作，并进行评分选择。
5. **角色与阵型层**：根据球员号码、比赛局势和阵型文件分配位置职责。
6. **ML 推理增强层**：使用导入的 MLP 权重对无球跑位和射门机会进行额外评分。

其中 ML 推理增强层是本版本相对 `BoltV6` 的主要新增部分。

## 4. 关键新增模型

### 4.1 无球跑位模型

- 权重文件：`unmark_mlp_weights.txt`
- 元信息文件：`unmark_mlp_weights.json`
- C++ 推理文件：`src/player/learning/bolt_unmark_inference.cpp`
- 接入位置：`src/player/bhv_unmark.cpp`

该模型用于评估一个候选无球跑位点是否更容易接应传球。原始 `BoltV6` 主要依赖手写启发式规则，例如传球可达性、对手距离、转身角度、是否前插等因素。`BoltV6_ML` 在这些启发式评分基础上额外加入模型输出，作为候选跑位点的残差加分。

当前接入方式为：

```cpp
sum_eval += LAMBDA_UNMARK * BoltUnmarkInference::score(...);
```

这意味着模型不会完全接管跑位决策，而是影响候选点排序。

### 4.2 射门目标模型

- 权重文件：`shot_target_mlp_weights.txt`
- 元信息文件：`shot_target_mlp_weights.json`
- C++ 推理文件：`src/player/learning/bolt_shot_inference.cpp`
- 接入位置：`src/player/sample_field_evaluator.cpp`

该模型用于辅助评估射门状态价值。它会针对球门中路和上下两个候选目标点进行评分，并将最佳模型分数加入场面评价值。

当前接入方式为：

```cpp
point += LAMBDA_SHOT * best_ml;
```

由于射门评分在行动链规划中权重较大，该模型可能明显影响进攻端是否更倾向于选择射门相关动作。

## 5. 运行时权重部署

为了保证程序在 `build/bin` 目录运行时可以直接读取模型，项目中保留了两类权重位置：

1. **模型归档位置**：
   - `models/exported/unmark_mlp_weights.txt`
   - `models/exported/shot_target_mlp_weights.txt`

2. **运行时复制源**：
   - `src/unmark_mlp_weights.txt`
   - `src/shot_target_mlp_weights.txt`

`src/CMakeLists.txt` 中已经配置在构建时将运行时权重复制到 `build/bin`，因此执行 `./start.sh` 时，客户端可以通过相对路径加载：

```cpp
./unmark_mlp_weights.txt
./shot_target_mlp_weights.txt
```

## 6. 主要代码改动说明

### 6.1 构建系统

`src/player/CMakeLists.txt` 增加了以下推理源文件：

- `learning/bolt_unmark_inference.cpp`
- `learning/bolt_shot_inference.cpp`

并将 `src/player/learning` 加入 include 路径。

`src/CMakeLists.txt` 增加了两个 ML 权重文件的构建复制逻辑。

### 6.2 无球行为

`src/player/bhv_unmark.cpp` 增加了：

- `BoltUnmarkInference` 头文件引用
- 启动时加载 `unmark_mlp_weights.txt`
- 在候选跑位点评价中加入 ML 评分

### 6.3 射门评价

`src/player/sample_player.cpp` 增加了启动阶段加载 `shot_target_mlp_weights.txt` 的逻辑。

`src/player/sample_field_evaluator.cpp` 增加了射门机会 ML 评分，将其作为场面评价的一部分。

## 7. 队伍行为特点

相比原始 `BoltV6`，`BoltV6_ML` 的潜在行为变化主要体现在：

1. **接应跑位更受数据驱动影响**：候选跑位点不仅看手写规则，也参考模型对“可接应性”的判断。
2. **进攻选择更重视射门质量**：当模型认为某些射门目标质量较高时，行动链评价可能更偏向射门方向。
3. **保留原有战术稳定性**：模型只是附加评分，不直接替代原有规划器，因此整体行为仍保持 `BoltV6` 风格。
4. **可渐进调参**：`LAMBDA_UNMARK` 和 `LAMBDA_SHOT` 是影响模型强度的重要参数，可通过比赛测试继续调整。

## 8. 已完成验证

当前版本已经完成以下本地验证：

1. `cmake ..` 配置成功。
2. `make -j2` 编译成功。
3. `build/bin/sample_player` 正常生成。
4. `build/bin` 中存在运行所需权重文件。
5. 已与原始 `BoltV6` 进行本地 RoboCup 2D 可视化对战测试。
6. 在正常自动裁判模式下，比赛可以进入 `play_on`，并出现正常的 `dash`、`kick`、`free_kick`、`kick_in` 等比赛事件。

## 9. 已知注意事项

1. 启动 `rcssserver` 时不要使用 `server::coach=true` 且不启用裁判，否则自动裁判会被关闭，可能导致比赛卡在开球、出界不判罚等状态。
2. 推荐使用：

```bash
rcssserver server::coach=false
```

3. 两队左右顺序取决于哪一队先连接 server。如果需要固定左右队，应先启动左队并等待 11 人连接，再启动右队。
4. 当前 ML 模型为 CppDNN 文本权重格式，运行时必须保证对应 `.txt` 权重在 `build/bin` 中。
5. 当前模型权重已经直接纳入项目，如果后续权重变大，需要考虑 Git LFS 或 release artifact 管理。

## 10. 构建与运行方式

### 构建

```bash
cd /home/ubuntu/robocup/BoltV6_ML
mkdir -p build
cd build
cmake ..
make -j2
```

### 启动队伍

```bash
cd /home/ubuntu/robocup/BoltV6_ML/build/bin
./start.sh -C -t BoltV6_ML -h localhost -p 6000
```

其中：

- `-C` 表示不启动在线 coach。
- `-t BoltV6_ML` 指定队名。
- `-p 6000` 是默认 server 端口，可根据实际比赛端口调整。

## 11. 后续优化建议

1. **模型权重调参**：通过多局对战统计调整 `LAMBDA_UNMARK` 和 `LAMBDA_SHOT`。
2. **增加开关配置**：为 ML 推理增加配置开关，方便比赛中对比启用/禁用效果。
3. **记录模型贡献**：在 debug 日志中记录 ML 分数，便于分析模型是否真正影响决策。
4. **扩大测试样本**：进行多场 `BoltV6_ML` vs `BoltV6`、左右互换、不同随机种子的测试。
5. **优化射门目标采样**：当前射门模型只评估中路和上下两个目标点，未来可以扩展为更密集的球门采样。
6. **接入更多模型**：后续可考虑对传球选择、防守站位、抢断时机等模块加入类似的 ML 残差评分。

## 12. 总结

`BoltV6_ML` 是一个以 `BoltV6` 为基础、引入机器学习推理增强的 RoboCup 2D 球队版本。它保留了原队伍成熟的规则和战术系统，同时把 `ml` 分支训练得到的无球跑位与射门目标模型接入到关键评价函数中。

从工程角度看，该版本已经具备完整的构建、运行和对战测试条件。从比赛策略角度看，它适合作为后续模型调参、对战评估和行为分析的基础版本。

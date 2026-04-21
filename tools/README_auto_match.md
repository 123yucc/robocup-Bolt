# Bolt Team - 自动化循环比赛系统

## 概述

用于 RoboCup 2D 强化学习数据收集的自动化比赛系统，解决手动开球问题。

## 核心功能

1. **自动循环比赛** - 运行N场比赛，无需手动干预
2. **自动开球** - 通过 Coach 发送 `(play_on)` 指令，替代 Ctrl+K
3. **数据收集** - 每场比赛结束后自动提取 CSV 数据
4. **进程管理** - 自动清理进程，避免残留

## 文件清单

```
Cyrus2DBase/tools/
├── auto_match_loop_v2.sh       # 主控制脚本（推荐使用）
├── auto_match_loop.sh          # 原版脚本（备份）
├── auto_match_coach.py         # 自动开球 Coach
├── extract_match_data.py       # 数据提取脚本
└── README_auto_match.md        # 说明文档
```

## 使用步骤

### 1. 确保环境准备好

```bash
# 检查 rcssserver 是否安装
rcssserver --version

# 如果未安装
sudo apt-get install rcssserver

# 确保球队已编译
cd /home/linna/Cyrus2DBase
mkdir -p build && cd build
cmake .. && make -j$(nproc)
```

### 2. 修改配置参数

编辑 `auto_match_loop_v2.sh` 顶部配置区域：

```bash
NUM_MATCHES=50              # 比赛场次
TEAM_LEFT_NAME="BoltRL"     # 新版本（RL增强）
TEAM_RIGHT_NAME="BoltBase"  # 旧版本或对手
MATCH_DURATION=600          # 比赛时长（秒）
```

### 3. 运行自动化比赛

```bash
cd /home/linna/Cyrus2DBase
chmod +x tools/auto_match_loop_v2.sh
./tools/auto_match_loop_v2.sh
```

### 4. 检查结果

```bash
# 查看数据文件
ls data/matches/

# 查看合并数据
head data/matches/all_matches_data.csv

# 查看比赛日志
ls logs/match_*.rcg
```

## 关键技术点

### 1. 自动开球原理

RoboCup server 默认需要手动按 `Ctrl+K` 开始比赛。解决方案：

**方案A（推荐）**：使用 Coach 发送 `(play_on)` 指令
```python
# auto_match_coach.py
# 连接server后发送:
kickoff_msg = "(play_on)"
socket.sendto(kickoff_msg.encode(), server_addr)
```

**方案B**：使用 rcssserver 的 `-T` 参数（Trainer模式）
```bash
rcssserver -T -p 6000  # 允许 Coach 发送指令
```

### 2. 数据提取流程

```
.rcg日志 → 解析 → 提取特征 → 计算奖励 → CSV文件
```

特征包含：
- 球位置和速度（4维）
- 球员位置（44维）
- 比分（2维）
- 游戏模式（4维）
- 扩展特征（剩余维度）

奖励函数：
- 进球: +1000
- 失球: -1000
- 球推进: +5 * x_change
- 防守压力: -20 * danger_factor

### 3. 进程管理

脚本会自动：
- 清理残留进程（server, player, coach）
- 检查端口是否释放
- 等待进程完全退出

## 常见问题

### Q1: 端口被占用

```bash
# 检查端口占用
netstat -tuln | grep 6000

# 强制清理
pkill -9 -f "rcssserver"
sleep 5
```

### Q2: 球队连接失败

检查球队二进制是否存在：
```bash
ls -l /home/linna/Cyrus2DBase/bin/sample_player
```

### Q3: 比赛不开始（仍需手动开球）

确保：
1. Server 启动时加了 `-T` 参数
2. Coach 脚本正确发送了 `(play_on)` 指令
3. 两队都已连接

检查 Coach 日志：
```bash
cat logs/coach_1.log
```

### Q4: 数据文件为空或很小

可能原因：
- 比赛时间太短
- RCG文件解析失败
- 球队异常退出

检查：
```bash
# 检查RCG文件大小
ls -lh logs/match_1.rcg

# 手动运行数据提取测试
python3 tools/extract_match_data.py --rcg logs/match_1.rcg --output test.csv
```

### Q5: 进程残留导致下一场失败

脚本有自动清理，但如果仍然有问题：
```bash
# 手动全面清理
pkill -9 -f "rcssserver"
pkill -9 -f "sample_player"
pkill -9 -f "sample_coach"
pkill -9 -f "rcssmonitor"
sleep 10
```

## 性能优化建议

### 1. 并行化

如果有多台机器，可以：
- 在不同机器上运行不同端口
- 使用分布式训练

### 2. 数据存储优化

```bash
# 使用压缩
gzip data/matches/match_*_data.csv

# 定期归档
mv data/matches/*.csv data/archive/
```

### 3. 资源监控

```bash
# 监控进程数量
ps aux | grep sample_player | wc -l

# 监控内存使用
top -p $(pgrep -d',' -f "rcssserver|sample_player")
```

## 扩展功能

### 1. 对战不同对手

修改配置使用不同的球队：
```bash
TEAM_RIGHT_DIR="/path/to/opponent_team"
TEAM_RIGHT_BIN="bin/opponent_player"
```

### 2. 自定义数据提取

修改 `extract_match_data.py`：
- 添加更多特征维度
- 自定义奖励函数
- 添加特定事件标注

### 3. 实时监控

启动 rcssmonitor 观看比赛：
```bash
# 每场比赛启动monitor（可视化）
rcssmonitor -p 6000 &
```

## 示例输出

```
[INFO] =======================================
[INFO] Bolt 自动化比赛循环系统
[INFO] =======================================
[INFO] 总场次: 50
[INFO] 左侧球队: BoltRL
[INFO] 右侧球队: BoltBase

[INFO] ===================================== Match 1 / 50 =================================
[STEP] 启动 Server (Match 1)...
[INFO] Server已启动 (PID: 12345)
[STEP] 启动 left 球队 (BoltRL)...
[INFO] left 球队已启动 (11名球员)
[STEP] 启动 right 球队 (BoltBase)...
[INFO] right 球队已启动 (11名球员)
[STEP] 启动自动开球 Coach...
[INFO] 自动开球 Coach已启动 (PID: 12399)
[INFO] 比赛将自动开始，无需手动 Ctrl+K
[STEP] 等待比赛结束...
[INFO] 比赛已开始！
[STEP] 收集比赛数据...
[INFO] 数据已保存: data/matches/match_1_data.csv (5234 行)
[INFO] Match 1 结果: BoltRL 2 - 1 BoltBase
[INFO] Match 1: 胜利！

...

[INFO] =======================================
[INFO] 所有比赛完成！
[INFO] =======================================
[INFO] 总场次: 50
[INFO] BoltRL: 30胜 5平 15负
[INFO] 总进球: BoltRL 85 - 45 BoltBase
[INFO] 数据保存: data/matches
[INFO] =======================================
```

## 下一步

有了训练数据后：

1. **训练模型**
```bash
python3 scripts/training_rl/train_rl_agent.py --data data/matches/all_matches_data.csv
```

2. **转换权重**
```bash
python3 scripts/training_rl/convert_weights_to_cppdnn.py --model trained_model.h5 --output weights/rl_value.txt
```

3. **加载权重**
修改 `sample_player.cpp` 加载训练好的权重：
```cpp
evaluator->loadWeights("weights/rl_value.txt", "weights/rl_policy.txt");
```

4. **测试效果**
再次运行比赛，观察 RL 增强的效果。

## 联系

如有问题，检查日志文件或查看脚本输出的错误信息。
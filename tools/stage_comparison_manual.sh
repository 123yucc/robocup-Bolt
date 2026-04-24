#!/bin/bash

# RoboCup 2D 阶段对比测试（手动模式）
# 阶段三 (Cyrus2DBase) vs 阶段二 (robocup-Bolt)

STAGE3_DIR="/home/linna/Cyrus2DBase/build/bin"
STAGE2_DIR="/home/linna/robocup-Bolt/build/bin"

export LD_LIBRARY_PATH="/home/linna/local/lib:$LD_LIBRARY_PATH"

echo "=== 阶段对比测试（手动模式） ==="
echo ""
echo "左边队: 阶段三 (Cyrus2DBase)"
echo "  - 阶段2: 射门角度优化 + 传球拦截惩罚/直塞奖励"
echo "  - 阶段3: 门将主动出击 + 盯人防守 + 动态防线调整"
echo "  目录: $STAGE3_DIR"
echo ""
echo "右边队: 阶段二 (robocup-Bolt)"
echo "  - 阶段2: 射门角度优化 + 传球拦截惩罚/直塞奖励"
echo "  - 阶段3: 未实现（防守为原始版本）"
echo "  目录: $STAGE2_DIR"
echo ""

# 清理旧进程
pkill -f rcssserver 2>/dev/null || true
pkill -f sample_player 2>/dev/null || true
pkill -f sample_coach 2>/dev/null || true
pkill -f rcssmonitor 2>/dev/null || true
sleep 2

# 1. 启动 rcssserver
echo "[1/4] 启动 rcssserver..."
rcssserver &
SERVER_PID=$!
sleep 2

# 2. 启动阶段三队 (Cyrus2DBase - 左边)
echo "[2/4] 启动阶段三队 (Cyrus2DBase)..."
cd "$STAGE3_DIR"
./start.sh &
STAGE3_PID=$!
sleep 3

# 3. 启动阶段二队 (robocup-Bolt - 右边)
echo "[3/4] 启动阶段二队 (Stage2)..."
cd "$STAGE2_DIR"
./start.sh -t Stage2 &
STAGE2_PID=$!
sleep 2

# 4. 启动 monitor
echo "[4/4] 启动 rcssmonitor..."
rcssmonitor &
MONITOR_PID=$!

echo ""
echo "=== 比赛已准备就绪 ==="
echo ""
echo "操作说明:"
echo "  1. 在 monitor 中按 Ctrl+C 连接服务器"
echo "  2. 按 Ctrl+K 开球"
echo "  3. 比赛结束后关闭此窗口即可"
echo ""
echo "进程 PID:"
echo "  Server: $SERVER_PID"
echo "  Stage3 (Cyrus2DBase): $STAGE3_PID"
echo "  Stage2: $STAGE2_PID"
echo "  Monitor: $MONITOR_PID"
echo ""

# 等待
wait
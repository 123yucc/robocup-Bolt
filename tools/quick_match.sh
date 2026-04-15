#!/bin/bash

# RoboCup 2D 单场快速对战脚本
# Bolt 队 vs Opponent 队

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BIN_DIR="$BUILD_DIR/bin"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Bolt 单场快速对战脚本 ===${NC}"

# 1. 后台启动 rcssserver
echo -e "${YELLOW}[1/4] 启动 rcssserver...${NC}"
cd "$PROJECT_ROOT"
rcssserver > /tmp/rcssserver.log 2>&1 &
SERVER_PID=$!
echo -e "${GREEN}rcssserver 已启动 (PID: $SERVER_PID)${NC}"

# 等待 server 启动
sleep 1

# 2. 后台运行本队 (Bolt)
echo -e "${YELLOW}[2/4] 启动 Bolt 队...${NC}"
cd "$BIN_DIR"
./start.sh > /tmp/bolt.log 2>&1 &
BOLT_PID=$!
echo -e "${GREEN}Bolt 队已启动 (PID: $BOLT_PID)${NC}"

# 等待 Bolt 队连接
sleep 2

# 3. 后台运行对手 (Opponent)
echo -e "${YELLOW}[3/4] 启动 Opponent 队...${NC}"
./start.sh -t Opponent > /tmp/opponent.log 2>&1 &
OPPONENT_PID=$!
echo -e "${GREEN}Opponent 队已启动 (PID: $OPPONENT_PID)${NC}"

# 等待 Opponent 队连接
sleep 1

# 4. 后台启动 soccerwindow2 用于观赛
echo -e "${YELLOW}[4/4] 启动 soccerwindow2...${NC}"
soccerwindow2 > /tmp/soccerwindow2.log 2>&1 &
WINDOW_PID=$!
echo -e "${GREEN}soccerwindow2 已启动 (PID: $WINDOW_PID)${NC}"

echo ""
echo -e "${GREEN}=== 比赛已开始 ===${NC}"
echo "Bolt 队 PID: $BOLT_PID"
echo "Opponent 队 PID: $OPPONENT_PID"
echo "soccerwindow2 PID: $WINDOW_PID"
echo ""
echo -e "${YELLOW}按 Ctrl+C 停止比赛${NC}"
echo ""

# 等待所有进程
wait $BOLT_PID $OPPONENT_PID

# 比赛结束后清理
echo ""
echo -e "${YELLOW}=== 比赛结束 ===${NC}"
echo "清理进程..."
kill $SERVER_PID 2>/dev/null || true
kill $WINDOW_PID 2>/dev/null || true
sleep 1

echo -e "${GREEN}=== 脚本执行完成 ===${NC}"

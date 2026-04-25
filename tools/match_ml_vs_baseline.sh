#!/bin/bash

# Bolt (当前版本，有ML) vs Bolt (基线版本 3459d1b，无ML) 对战脚本

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BOLT_ML_DIR="$PROJECT_ROOT/build/bin"
BOLT_BASELINE_DIR="/tmp/bolt_baseline_build/build/bin"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${GREEN}=== Bolt ML vs Bolt Baseline 对战 ===${NC}"
echo -e "${BLUE}左侧: Bolt ML (当前版本，有机器学习)${NC}"
echo -e "${BLUE}右侧: Bolt Baseline (commit 3459d1b，无机器学习)${NC}"
echo ""

# 检查基线版本是否存在
if [ ! -f "$BOLT_BASELINE_DIR/sample_player" ]; then
    echo -e "${RED}错误: 基线版本不存在${NC}"
    echo "路径: $BOLT_BASELINE_DIR/sample_player"
    exit 1
fi

# 1. 启动 rcssserver
echo -e "${YELLOW}[1/4] 启动 rcssserver...${NC}"
cd "$PROJECT_ROOT"
rcssserver > /tmp/rcssserver.log 2>&1 &
SERVER_PID=$!
echo -e "${GREEN}rcssserver 已启动 (PID: $SERVER_PID)${NC}"

sleep 1

# 2. 启动 Bolt ML 队（左侧）
echo -e "${YELLOW}[2/4] 启动 Bolt ML 队 (左侧)...${NC}"
cd "$BOLT_ML_DIR"
./start.sh -t BoltML > /tmp/bolt_ml.log 2>&1 &
BOLT_ML_PID=$!
echo -e "${GREEN}Bolt ML 队已启动 (PID: $BOLT_ML_PID)${NC}"

sleep 2

# 3. 启动 Bolt Baseline 队（右侧）
echo -e "${YELLOW}[3/4] 启动 Bolt Baseline 队 (右侧)...${NC}"
cd "$BOLT_BASELINE_DIR"
./start.sh -t BoltBase > /tmp/bolt_baseline.log 2>&1 &
BOLT_BASE_PID=$!
echo -e "${GREEN}Bolt Baseline 队已启动 (PID: $BOLT_BASE_PID)${NC}"

sleep 1

# 4. 启动 rcssmonitor
echo -e "${YELLOW}[4/4] 启动 rcssmonitor...${NC}"
if command -v rcssmonitor &> /dev/null; then
    rcssmonitor > /tmp/rcssmonitor.log 2>&1 &
    MONITOR_PID=$!
    echo -e "${GREEN}rcssmonitor 已启动 (PID: $MONITOR_PID)${NC}"
else
    echo -e "${YELLOW}rcssmonitor 未安装，跳过可视化${NC}"
    MONITOR_PID=""
fi

echo ""
echo -e "${GREEN}=== 比赛已开始 ===${NC}"
echo -e "${BLUE}BoltML (左侧, 有ML)${NC} vs ${YELLOW}BoltBase (右侧, 无ML)${NC}"
echo "BoltML PID: $BOLT_ML_PID"
echo "BoltBase PID: $BOLT_BASE_PID"
if [ -n "$MONITOR_PID" ]; then
    echo "Monitor PID: $MONITOR_PID"
fi
echo ""
echo "日志文件:"
echo "  - Bolt ML:       /tmp/bolt_ml.log"
echo "  - Bolt Baseline: /tmp/bolt_baseline.log"
echo "  - Server:        /tmp/rcssserver.log"
echo ""
echo -e "${YELLOW}按 Ctrl+C 停止比赛${NC}"
echo ""

# 等待服务器进程结束（比赛完成）
wait $SERVER_PID

# 比赛结束后清理
echo ""
echo -e "${YELLOW}=== 比赛结束 ===${NC}"
echo "清理进程..."
kill $BOLT_ML_PID $BOLT_BASE_PID 2>/dev/null || true
if [ -n "$MONITOR_PID" ]; then
    kill $MONITOR_PID 2>/dev/null || true
fi
sleep 1

# 显示比分
echo ""
echo -e "${GREEN}=== 比赛结果 ===${NC}"
LATEST_RCG=$(ls -t ~/.rcssserver/*.rcg 2>/dev/null | head -1 || echo "")
if [ -n "$LATEST_RCG" ]; then
    echo "比赛录像: $LATEST_RCG"
fi

echo ""
echo -e "${GREEN}=== 对战完成 ===${NC}"

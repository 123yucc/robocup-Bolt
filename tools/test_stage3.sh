#!/bin/bash

# 阶段3优化效果测试脚本
# BoltV3 (阶段2+3) vs BoltV2 (仅阶段2)

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  阶段3优化效果测试                                         ║${NC}"
echo -e "${CYAN}║  BoltV3 (阶段2+3) vs BoltV2 (仅阶段2)                      ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# 清理旧进程
pkill -9 rcssserver 2>/dev/null || true
pkill -9 sample_player 2>/dev/null || true
pkill -9 rcssmonitor 2>/dev/null || true
sleep 1

# 创建临时目录用于BoltV2
TEMP_DIR="/tmp/robocup_bolt_v2"
rm -rf "$TEMP_DIR"
mkdir -p "$TEMP_DIR"

echo -e "${YELLOW}[1/5] 准备BoltV2（仅阶段2）...${NC}"
cd /home/linna/robocup-Bolt
git stash push -m "temp stash for testing"
git checkout ddca4a9  # 阶段2提交
mkdir -p build_v2
cd build_v2
cmake .. > /dev/null 2>&1
make -j4 > /dev/null 2>&1
cp -r bin "$TEMP_DIR/"
cd /home/linna/robocup-Bolt
git checkout cyrus2d
git stash pop || true
echo -e "${GREEN}✓ BoltV2 准备完成${NC}"

echo ""
echo -e "${YELLOW}[2/5] 启动 rcssserver...${NC}"
rcssserver > /tmp/server_test.log 2>&1 &
SERVER_PID=$!
sleep 2
echo -e "${GREEN}✓ Server 已启动 (PID: $SERVER_PID)${NC}"

echo ""
echo -e "${YELLOW}[3/5] 启动 BoltV3 (阶段2+3优化版，左侧)...${NC}"
cd /home/linna/robocup-Bolt/build/bin
./start.sh -t BoltV3 > /tmp/bolt_v3_test.log 2>&1 &
BOLT_V3_PID=$!
sleep 3
echo -e "${GREEN}✓ BoltV3 已启动 (PID: $BOLT_V3_PID)${NC}"

echo ""
echo -e "${YELLOW}[4/5] 启动 BoltV2 (仅阶段2，右侧)...${NC}"
cd "$TEMP_DIR/bin"
./start.sh -t BoltV2 > /tmp/bolt_v2_test.log 2>&1 &
BOLT_V2_PID=$!
sleep 3
echo -e "${GREEN}✓ BoltV2 已启动 (PID: $BOLT_V2_PID)${NC}"

echo ""
echo -e "${YELLOW}[5/5] 启动 rcssmonitor...${NC}"
rcssmonitor > /tmp/monitor_test.log 2>&1 &
MONITOR_PID=$!
sleep 1
echo -e "${GREEN}✓ rcssmonitor 已启动 (PID: $MONITOR_PID)${NC}"

echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  测试环境已就绪！                                          ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${BLUE}对战配置:${NC}"
echo -e "  ${GREEN}左侧 (BoltV3):${NC} 阶段2+3优化"
echo -e "    ✓ 搜索深度: 6步"
echo -e "    ✓ 评估次数: 1000次"
echo -e "    ✓ 射门采样: 50点"
echo -e "    ✓ 智能守门员出击"
echo ""
echo -e "  ${YELLOW}右侧 (BoltV2):${NC} 仅阶段2优化"
echo -e "    ✓ 搜索深度: 4步"
echo -e "    ✓ 评估次数: 500次"
echo -e "    ✓ 射门采样: 50点"
echo -e "    ✗ 基础守门员"
echo ""
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}请在 rcssmonitor 中按 Ctrl+K 开球！${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${YELLOW}比赛结束后按 Ctrl+C 查看结果${NC}"
echo ""

# 清理函数
cleanup() {
    echo ""
    echo -e "${YELLOW}正在停止比赛...${NC}"

    kill $BOLT_V3_PID 2>/dev/null || true
    kill $BOLT_V2_PID 2>/dev/null || true
    kill $MONITOR_PID 2>/dev/null || true
    sleep 1
    kill $SERVER_PID 2>/dev/null || true
    sleep 1

    echo ""
    echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║  比赛结果分析                                              ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
    echo ""

    if [ -f /tmp/server_test.log ]; then
        GOALS_L=$(grep -oP "goal_l \d+" /tmp/server_test.log | grep -oP "\d+" | tail -1)
        GOALS_R=$(grep -oP "goal_r \d+" /tmp/server_test.log | grep -oP "\d+" | tail -1)

        if [ -n "$GOALS_L" ] && [ -n "$GOALS_R" ]; then
            echo -e "${BLUE}最终比分:${NC}"
            echo -e "  ${GREEN}BoltV3 (阶段2+3):${NC} $GOALS_L"
            echo -e "  ${YELLOW}BoltV2 (仅阶段2):${NC} $GOALS_R"
            echo ""

            if [ "$GOALS_L" -gt "$GOALS_R" ]; then
                DIFF=$((GOALS_L - GOALS_R))
                echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
                echo -e "${GREEN}║  BoltV3 获胜！净胜 $DIFF 球                                 ║${NC}"
                echo -e "${GREEN}║  阶段3优化有效！                                           ║${NC}"
                echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}"
            elif [ "$GOALS_L" -lt "$GOALS_R" ]; then
                DIFF=$((GOALS_R - GOALS_L))
                echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
                echo -e "${RED}║  BoltV2 获胜，净胜 $DIFF 球                                 ║${NC}"
                echo -e "${RED}║  需要检查阶段3优化                                         ║${NC}"
                echo -e "${RED}╚════════════════════════════════════════════════════════════╝${NC}"
            else
                echo -e "${YELLOW}╔════════════════════════════════════════════════════════════╗${NC}"
                echo -e "${YELLOW}║  平局！                                                    ║${NC}"
                echo -e "${YELLOW}╚════════════════════════════════════════════════════════════╝${NC}"
            fi

            # 检查守门员出击
            if [ -f /tmp/bolt_v3_test.log ]; then
                RUSH_COUNT=$(grep -c "SmartRush" /tmp/bolt_v3_test.log 2>/dev/null || echo "0")
                if [ "$RUSH_COUNT" -gt 0 ]; then
                    echo ""
                    echo -e "${GREEN}✓ BoltV3守门员智能出击次数: $RUSH_COUNT${NC}"
                fi
            fi
        fi
    fi

    echo ""
    echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}测试完成！${NC}"
    echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"

    # 清理临时目录
    rm -rf "$TEMP_DIR"
}

trap cleanup SIGINT SIGTERM

# 等待用户中断
wait $SERVER_PID 2>/dev/null || true
cleanup

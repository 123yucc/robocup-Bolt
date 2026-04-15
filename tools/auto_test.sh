#!/bin/bash

# RoboCup 2D 批量自动测试脚本
# Bolt 队 vs Opponent 队

set -e

# 默认参数
TEAM_DIR="/home/linna/Cyrus2DBase"
OPPONENT_DIR="/home/linna/Cyrus2DBase"
TEST_MATCHES=10

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--team-dir)
            TEAM_DIR="$2"
            shift 2
            ;;
        -o|--opponent-dir)
            OPPONENT_DIR="$2"
            shift 2
            ;;
        -n|--num-matches)
            TEST_MATCHES="$2"
            shift 2
            ;;
        *)
            echo "未知参数: $1"
            echo "用法: $0 [-t TEAM_DIR] [-o OPPONENT_DIR] [-n TEST_MATCHES]"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BIN_DIR="$BUILD_DIR/bin"

# 设置库路径
export LD_LIBRARY_PATH="/home/linna/local/lib:$LD_LIBRARY_PATH"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 创建测试结果目录
TEST_DIR="/tmp/robocup_2d_test_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEST_DIR"
CSV_FILE="$TEST_DIR/match_results.csv"

# CSV 文件头
echo "Match,BeforeKickOff,PlayOn,GoalsL,GoalsR,TimeL,TimeR" > "$CSV_FILE"

echo -e "${BLUE}=== RoboCup 2D 批量自动测试脚本 ===${NC}"
echo "队伍目录: $TEAM_DIR"
echo "对手目录: $OPPONENT_DIR"
echo "测试场次: $TEST_MATCHES"
echo "结果目录: $TEST_DIR"
echo "CSV 文件: $CSV_FILE"
echo ""

# 统计变量
TOTAL_MATCHES=0
WINS=0
DRAWS=0
LOSSES=0
GOALS_FOR=0
GOALS_AGAINST=0

# 循环启动比赛
for (( i=1; i<=$TEST_MATCHES; i++ )); do
    echo -e "${YELLOW}=== 第 $i/$TEST_MATCHES 场比赛 ===${NC}"

    # 清理旧日志
    rm -f /tmp/rcssserver.log /tmp/bolt.log /tmp/opponent.log

    # 启动 server (手动模式)
    rcssserver \
        server::coach_mode=on \
        server::synch_mode=on \
        server::nr_normal_halfs=2 \
        server::fullstate_l=false \
        server::fullstate_r=false \
        > /tmp/rcssserver.log 2>&1 &
    SERVER_PID=$!
    echo "Server PID: $SERVER_PID"

    # 等待 server 完全启动（检查端口是否开放）
    echo "等待 server 启动..."
    for i in {1..30}; do
        if timeout 1 bash -c "echo > /dev/tcp/localhost/6000" 2>/dev/null; then
            echo "Server 已启动"
            break
        fi
        sleep 1
    done

    # 等待 server 稳定
    sleep 2

    # 启动 Bolt 队
    cd "$BIN_DIR"
    ./start.sh > /tmp/bolt.log 2>&1 &
    BOLT_PID=$!
    echo "Bolt PID: $BOLT_PID"

    # 等待 Bolt 连接（检查日志中是否有连接成功的消息）
    echo "等待 Bolt 队连接..."
    for i in {1..30}; do
        if grep -q "connected to server" /tmp/bolt.log 2>/dev/null; then
            echo "Bolt 队已连接"
            break
        fi
        sleep 1
    done

    # 等待额外的稳定时间
    sleep 2

    # 启动 Opponent 队
    ./start.sh -t Opponent > /tmp/opponent.log 2>&1 &
    OPPONENT_PID=$!
    echo "Opponent PID: $OPPONENT_PID"

    # 等待 Opponent 连接（检查日志中是否有连接成功的消息）
    echo "等待 Opponent 队连接..."
    for i in {1..30}; do
        if grep -q "connected to server" /tmp/opponent.log 2>/dev/null; then
            echo "Opponent 队已连接"
            break
        fi
        sleep 1
    done

    # 等待额外的稳定时间
    sleep 2

    # 等待比赛结束（增加超时时间）
    sleep 60
    WAIT_PID=$!

    # 检查所有进程是否还在运行
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}错误: Server 进程已退出${NC}"
        echo "Server 日志:"
        tail -20 /tmp/rcssserver.log
        kill $BOLT_PID 2>/dev/null || true
        kill $OPPONENT_PID 2>/dev/null || true
        sleep 1
        continue
    fi

    if ! kill -0 $BOLT_PID 2>/dev/null; then
        echo -e "${RED}错误: Bolt 进程已退出${NC}"
        echo "Bolt 日志:"
        tail -20 /tmp/bolt.log
        kill $SERVER_PID 2>/dev/null || true
        kill $OPPONENT_PID 2>/dev/null || true
        sleep 1
        continue
    fi

    if ! kill -0 $OPPONENT_PID 2>/dev/null; then
        echo -e "${RED}错误: Opponent 进程已退出${NC}"
        echo "Opponent 日志:"
        tail -20 /tmp/opponent.log
        kill $SERVER_PID 2>/dev/null || true
        kill $BOLT_PID 2>/dev/null || true
        sleep 1
        continue
    fi

    # 等待 server 和窗口进程
    sleep 3

    # 检查比赛日志中的比分
    BEFORE_KICKOFF=""
    PLAY_ON=""
    GOALS_L=""
    GOALS_R=""
    TIME_L=""
    TIME_R=""

    if [ -f /tmp/rcssserver.log ]; then
        # 解析日志
        BEFORE_KICKOFF=$(grep -oP "BeforeKickOff" /tmp/rcssserver.log | wc -l)
        PLAY_ON=$(grep -oP "PlayOn" /tmp/rcssserver.log | wc -l)
        GOALS_L=$(grep -oP "goal_l \d+" /tmp/rcssserver.log | grep -oP "\d+" | tail -1)
        GOALS_R=$(grep -oP "goal_r \d+" /tmp/rcssserver.log | grep -oP "\d+" | tail -1)
    fi

    # 写入 CSV
    echo "$i,$BEFORE_KICKOFF,$PLAY_ON,$GOALS_L,$GOALS_R,$TIME_L,$TIME_R" >> "$CSV_FILE"

    # 统计
    TOTAL_MATCHES=$((TOTAL_MATCHES + 1))

    if [ -n "$GOALS_L" ] && [ -n "$GOALS_R" ]; then
        if [ "$GOALS_L" -gt "$GOALS_R" ]; then
            WINS=$((WINS + 1))
            GOALS_FOR=$((GOALS_FOR + GOALS_L))
            GOALS_AGAINST=$((GOALS_AGAINST + GOALS_R))
            echo -e "${GREEN}  结果: Bolt 胜 $GOALS_L - $GOALS_R${NC}"
        elif [ "$GOALS_L" -lt "$GOALS_R" ]; then
            LOSSES=$((LOSSES + 1))
            GOALS_FOR=$((GOALS_FOR + GOALS_L))
            GOALS_AGAINST=$((GOALS_AGAINST + GOALS_R))
            echo -e "${RED}  结果: Bolt 负 $GOALS_L - $GOALS_R${NC}"
        else
            DRAWS=$((DRAWS + 1))
            GOALS_FOR=$((GOALS_FOR + GOALS_L))
            GOALS_AGAINST=$((GOALS_AGAINST + GOALS_R))
            echo -e "${YELLOW}  结果: 平局 $GOALS_L - $GOALS_R${NC}"
        fi
    else
        echo -e "${YELLOW}  结果: 未完成比赛${NC}"
    fi

    # 清理进程
    kill $SERVER_PID 2>/dev/null || true
    sleep 1

    echo ""
done

# 输出统计结果
echo -e "${BLUE}=== 测试统计结果 ===${NC}"
echo ""
echo "总场次: $TOTAL_MATCHES"
echo -e "${GREEN}胜场: $WINS${NC}"
echo -e "${YELLOW}平场: $DRAWS${NC}"
echo -e "${RED}负场: $LOSSES${NC}"
echo ""

if [ $TOTAL_MATCHES -gt 0 ]; then
    WIN_RATE=$(awk "BEGIN {printf \"%.2f\", ($WINS/$TOTAL_MATCHES)*100}")
    echo "胜率: ${WIN_RATE}%"
    echo ""

    if [ $GOALS_FOR -gt 0 ] && [ $GOALS_AGAINST -gt 0 ]; then
        GOAL_DIFF=$(($GOALS_FOR - $GOALS_AGAINST))
        echo "进球数: Bolt $GOALS_FOR - Opponent $GOALS_AGAINST"
        echo "净胜球: $GOAL_DIFF"
    fi
fi

echo ""
echo -e "${GREEN}=== 所有测试完成 ===${NC}"
echo "详细结果已保存到: $CSV_FILE"
echo "日志文件: /tmp/rcssserver.log"

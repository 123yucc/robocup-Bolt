#!/bin/bash

# RoboCup 2D 阶段对比测试脚本
# 阶段三 (Bolt) vs 阶段二 (Stage2)

set -e

# 配置
STAGE3_DIR="/home/linna/robocup-Bolt/build/bin"
STAGE2_DIR="/tmp/bolt-stage2/build/bin"
NUM_MATCHES=${1:-10}

# 设置库路径
export LD_LIBRARY_PATH="/home/linna/local/lib:$LD_LIBRARY_PATH"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 创建测试结果目录
TEST_DIR="/tmp/robocup_stage_comparison_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$TEST_DIR"
CSV_FILE="$TEST_DIR/match_results.csv"

# CSV 文件头
echo "Match,Stage3Goals,Stage2Goals,Result,Duration" > "$CSV_FILE"

echo -e "${BLUE}=== RoboCup 2D 阶段对比测试 ===${NC}"
echo "阶段三目录: $STAGE3_DIR"
echo "阶段二目录: $STAGE2_DIR"
echo "测试场次: $NUM_MATCHES"
echo "结果目录: $TEST_DIR"
echo ""

# 统计变量
WINS=0
DRAWS=0
LOSSES=0
GOALS_STAGE3=0
GOALS_STAGE2=0

# 循环启动比赛
for (( i=1; i<=$NUM_MATCHES; i++ )); do
    echo -e "${YELLOW}=== 第 $i/$NUM_MATCHES 场比赛 ===${NC}"

    # 清理旧进程和日志
    pkill -f rcssserver 2>/dev/null || true
    pkill -f sample_player 2>/dev/null || true
    pkill -f sample_coach 2>/dev/null || true
    sleep 2

    rm -f /tmp/rcssserver.log /tmp/stage3.log /tmp/stage2.log

    # 启动 server
    echo "启动 rcssserver..."
    rcssserver \
        server::coach_mode=on \
        server::synch_mode=on \
        server::nr_normal_halfs=2 \
        server::fullstate_l=false \
        server::fullstate_r=false \
        > /tmp/rcssserver.log 2>&1 &
    SERVER_PID=$!

    # 等待 server 启动
    for j in {1..30}; do
        if timeout 1 bash -c "echo > /dev/tcp/localhost/6000" 2>/dev/null; then
            break
        fi
        sleep 1
    done
    sleep 1

    # 启动阶段三队 (Bolt - 左边)
    echo "启动阶段三队 (Bolt)..."
    cd "$STAGE3_DIR"
    ./start.sh > /tmp/stage3.log 2>&1 &
    STAGE3_PID=$!

    sleep 2

    # 启动阶段二队 (Stage2 - 右边，使用 Opponent 名称)
    echo "启动阶段二队 (Stage2)..."
    cd "$STAGE2_DIR"
    ./start.sh -t Stage2 > /tmp/stage2.log 2>&1 &
    STAGE2_PID=$!

    sleep 2

    # 等待比赛结束 (每半场约3000周期，同步模式约需60-90秒)
    echo "等待比赛结束..."
    MATCH_DURATION=0
    for j in {1..120}; do
        # 检查 server 是否还在运行
        if ! kill -0 $SERVER_PID 2>/dev/null; then
            echo "比赛结束"
            break
        fi

        # 检查日志中是否有结束标志
        if grep -q "time_over" /tmp/rcssserver.log 2>/dev/null; then
            echo "检测到比赛结束信号"
            sleep 3
            break
        fi

        sleep 1
        MATCH_DURATION=$((MATCH_DURATION + 1))

        # 每10秒打印进度
        if [ $((j % 10)) -eq 0 ]; then
            echo "  已等待 ${MATCH_DURATION} 秒..."
        fi
    done

    # 解析比分
    GOALS_L=""
    GOALS_R=""

    if [ -f /tmp/rcssserver.log ]; then
        # 从日志中提取比分
        GOALS_L=$(grep -oP "goal_l\s+\d+" /tmp/rcssserver.log | tail -1 | grep -oP "\d+" || echo "0")
        GOALS_R=$(grep -oP "goal_r\s+\d+" /tmp/rcssserver.log | tail -1 | grep -oP "\d+" || echo "0")

        # 如果没有找到goal记录，尝试从其他信息解析
        if [ -z "$GOALS_L" ] || [ -z "$GOALS_R" ]; then
            GOALS_L=0
            GOALS_R=0
        fi
    fi

    # 统计结果
    RESULT=""
    if [ -n "$GOALS_L" ] && [ -n "$GOALS_R" ]; then
        if [ "$GOALS_L" -gt "$GOALS_R" ]; then
            WINS=$((WINS + 1))
            GOALS_STAGE3=$((GOALS_STAGE3 + GOALS_L))
            GOALS_STAGE2=$((GOALS_STAGE2 + GOALS_R))
            RESULT="WIN"
            echo -e "${GREEN}  结果: 阶段三胜 $GOALS_L - $GOALS_R${NC}"
        elif [ "$GOALS_L" -lt "$GOALS_R" ]; then
            LOSSES=$((LOSSES + 1))
            GOALS_STAGE3=$((GOALS_STAGE3 + GOALS_L))
            GOALS_STAGE2=$((GOALS_STAGE2 + GOALS_R))
            RESULT="LOSS"
            echo -e "${RED}  结果: 阶段三负 $GOALS_L - $GOALS_R${NC}"
        else
            DRAWS=$((DRAWS + 1))
            GOALS_STAGE3=$((GOALS_STAGE3 + GOALS_L))
            GOALS_STAGE2=$((GOALS_STAGE2 + GOALS_R))
            RESULT="DRAW"
            echo -e "${YELLOW}  结果: 平局 $GOALS_L - $GOALS_R${NC}"
        fi
    else
        RESULT="UNKNOWN"
        GOALS_L=0
        GOALS_R=0
        echo -e "${YELLOW}  结果: 未完成比赛${NC}"
    fi

    # 写入 CSV
    echo "$i,$GOALS_L,$GOALS_R,$RESULT,$MATCH_DURATION" >> "$CSV_FILE"

    # 清理进程
    kill $SERVER_PID 2>/dev/null || true
    kill $STAGE3_PID 2>/dev/null || true
    kill $STAGE2_PID 2>/dev/null || true
    sleep 2

    echo ""
done

# 输出统计结果
echo -e "${BLUE}=== 阶段对比测试结果 ===${NC}"
echo ""
echo "总场次: $NUM_MATCHES"
echo -e "${GREEN}阶段三胜场: $WINS${NC}"
echo -e "${YELLOW}平局: $DRAWS${NC}"
echo -e "${RED}阶段三负场: $LOSSES${NC}"
echo ""

if [ $NUM_MATCHES -gt 0 ]; then
    WIN_RATE=$(awk "BEGIN {printf \"%.1f\", ($WINS/$NUM_MATCHES)*100}")
    echo "阶段三胜率: ${WIN_RATE}%"
    echo ""
    echo "总进球数:"
    echo "  阶段三: $GOALS_STAGE3"
    echo "  阶段二: $GOALS_STAGE2"

    if [ $GOALS_STAGE3 -gt 0 ] || [ $GOALS_STAGE2 -gt 0 ]; then
        GOAL_DIFF=$((GOALS_STAGE3 - GOALS_STAGE2))
        echo "  净胜球: $GOAL_DIFF"
    fi
fi

echo ""
echo -e "${GREEN}=== 测试完成 ===${NC}"
echo "详细结果: $CSV_FILE"

# 清理 worktree 提示
echo ""
echo "提示: 阶段二 worktree 位于 /tmp/bolt-stage2，测试后可删除"
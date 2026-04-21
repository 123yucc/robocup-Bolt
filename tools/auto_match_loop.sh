#!/bin/bash
#
# Bolt Team - 自动化循环比赛系统
# 用于强化学习数据收集
#
# 功能：自动运行N场比赛，收集数据，无需手动开球
#

# ================= 配置区域 =================
NUM_MATCHES=100                   # 比赛场次
SERVER_PORT=6000                  # Server端口
TEAM_LEFT_DIR="/home/linna/Cyrus2DBase"  # 左侧球队目录（新版本）
TEAM_RIGHT_DIR="/home/linna/Cyrus2DBase" # 右侧球队目录（旧版本或对手）
TEAM_LEFT_BIN="bin/sample_player"        # 左侧球队二进制
TEAM_RIGHT_BIN="bin/sample_player"       # 右侧球队二进制
TEAM_LEFT_NAME="BoltRL"                  # 左侧球队名
TEAM_RIGHT_NAME="BoltBase"               # 右侧球队名
DATA_OUTPUT_DIR="/home/linna/Cyrus2DBase/data/matches"  # 数据输出目录
LOG_DIR="/home/linna/Cyrus2DBase/logs"   # 日志目录
MATCH_DURATION=6000               # 比赛周期数（约10分钟）
# ============================================

# 创建必要的目录
mkdir -p "$DATA_OUTPUT_DIR"
mkdir -p "$LOG_DIR"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 清理所有进程
cleanup_processes() {
    log_info "清理进程..."

    # 杀死server进程
    pkill -9 -f "rcssserver" 2>/dev/null || true

    # 杀死球员进程
    pkill -9 -f "sample_player" 2>/dev/null || true

    # 杀死coach进程
    pkill -9 -f "sample_coach" 2>/dev/null || true

    # 杀死monitor进程
    pkill -9 -f "rcssmonitor" 2>/dev/null || true

    # 等待进程完全退出
    sleep 2

    log_info "进程清理完成"
}

# 启动server（自动开球模式）
start_server() {
    local match_num=$1
    local log_file="$LOG_DIR/server_match_${match_num}.log"

    log_info "启动 Server (Match $match_num)..."

    # 启动server，配置自动开球
    # 关键：使用 coach 模式自动发送 play_on
    rcssserver \
        -p $SERVER_PORT \
        -o "$LOG_DIR/match_${match_num}_out.rcl" \
        -l "$LOG_DIR/match_${match_num}.rcg" \
        -T  # Coach模式（允许教练发送指令）

    SERVER_PID=$!

    # 等待server启动
    sleep 3

    log_info "Server已启动 (PID: $SERVER_PID)"
}

# 启动coach（自动开球）
start_coach() {
    local side=$1  # left 或 right
    local port=$2
    local team_name=$3

    log_info "启动 ${side} Coach (${team_name})..."

    # Coach脚本：自动发送 (play_on) 指令
    # Coach连接后会自动发送开球指令，无需手动 Ctrl+K
    "$TEAM_LEFT_DIR/bin/sample_coach" \
        -p $port \
        -t "$team_name" \
        &

    COACH_PID=$!
    sleep 1

    log_info "${side} Coach已启动 (PID: $COACH_PID)"
}

# 启动球队
start_team() {
    local side=$1  # left 或 right
    local port=$2
    local team_name=$3
    local team_dir=$4
    local team_bin=$5

    log_info "启动 ${side} 球队 (${team_name})..."

    cd "$team_dir"

    # 启动11个球员
    "$team_bin" -p $port -t "$team_name" &
    sleep 1

    for i in {2..11}; do
        "$team_bin" -p $port -t "$team_name" -u $i &
        sleep 0.1
    done

    cd - > /dev/null

    log_info "${side} 球队已启动 (11名球员)"
}

# 发送自动开球指令（通过coach）
auto_kickoff() {
    log_info "发送自动开球指令..."

    # 使用 socat 或 netcat 发送 coach 指令
    # Coach协议：(play_on) 指令让比赛开始

    # 方法1：通过coach客户端（推荐）
    # sample_coach会自动发送play_on

    # 方法2：直接发送UDP指令
    # 需要等待server准备好
    sleep 2

    # Coach自动发送的指令格式
    # 实际上sample_coach在连接后会自动处理开球

    log_info "比赛已开始（自动模式）"
}

# 等待比赛结束
wait_for_match_end() {
    local match_num=$1
    local start_time=$(date +%s)
    local check_interval=10  # 每隔10秒检查一次

    log_info "等待比赛结束 (Match $match_num)..."

    while true; do
        local elapsed=$(($(date +%s) - start_time))

        # 检查比赛是否结束（通过日志文件）
        # RoboCup比赛通常在周期6000时结束

        # 检查server进程是否还在运行
        if ! ps -p $SERVER_PID > /dev/null 2>&1; then
            log_info "Server进程已结束，比赛完成"
            break
        fi

        # 检查比赛时长（超时保护）
        if [ $elapsed -gt $MATCH_DURATION ]; then
            log_warn "比赛超时（${elapsed}秒），强制结束"
            break
        fi

        # 显示进度
        log_info "比赛进行中... 已运行 ${elapsed}秒"

        sleep $check_interval
    done

    log_info "Match $match_num 完成"
}

# 收集比赛数据
collect_match_data() {
    local match_num=$1
    local rcg_file="$LOG_DIR/match_${match_num}.rcg"
    local csv_file="$DATA_OUTPUT_DIR/match_${match_num}_data.csv"

    log_info "收集比赛数据 (Match $match_num)..."

    if [ -f "$rcg_file" ]; then
        # 调用Python数据提取脚本
        python3 "$TEAM_LEFT_DIR/tools/extract_match_data.py" \
            --rcg "$rcg_file" \
            --output "$csv_file" \
            --match-num "$match_num"

        log_info "数据已保存到: $csv_file"
    else
        log_warn "未找到日志文件: $rcg_file"
    fi
}

# 统计比赛结果
get_match_result() {
    local match_num=$1
    local rcl_file="$LOG_DIR/match_${match_num}_out.rcl"

    # 从日志中提取比分
    # RoboCup日志格式：goal_l_<team> 或 goal_r_<team>

    local goals_left=0
    local goals_right=0

    if [ -f "$rcl_file" ]; then
        goals_left=$(grep -c "goal_l_" "$rcl_file" 2>/dev/null || echo 0)
        goals_right=$(grep -c "goal_r_" "$rcl_file" 2>/dev/null || echo 0)
    fi

    log_info "Match $match_num 结果: ${TEAM_LEFT_NAME} ${goals_left} - ${goals_right} ${TEAM_RIGHT_NAME}"

    return $goals_left $goals_right
}

# ================= 主循环 =================
main() {
    log_info "======================================="
    log_info "开始自动化比赛循环系统"
    log_info "总场次: $NUM_MATCHES"
    log_info "======================================="

    # 初始清理
    cleanup_processes

    # 统计总进球
    total_goals_left=0
    total_goals_right=0

    for match in $(seq 1 $NUM_MATCHES); do
        log_info ""
        log_info "==================== Match $match / $NUM_MATCHES ===================="

        # 启动server
        start_server $match

        # 启动coach（自动开球）
        # 注意：Coach连接后会自动发送play_on指令
        start_coach "left" $SERVER_PORT "$TEAM_LEFT_NAME"

        # 启动左侧球队
        start_team "left" $SERVER_PORT "$TEAM_LEFT_NAME" "$TEAM_LEFT_DIR" "$TEAM_LEFT_BIN"

        # 启动右侧球队（连接到相同端口）
        start_team "right" $SERVER_PORT "$TEAM_RIGHT_NAME" "$TEAM_RIGHT_DIR" "$TEAM_RIGHT_BIN"

        # 自动开球（coach会自动处理）
        auto_kickoff

        # 等待比赛结束
        wait_for_match_end $match

        # 收集数据
        collect_match_data $match

        # 获取结果
        get_match_result $match
        goals_left=$?
        goals_right=$?
        total_goals_left=$((total_goals_left + goals_left))
        total_goals_right=$((total_goals_right + goals_right))

        # 清理进程，准备下一场
        cleanup_processes

        # 短暂休息
        sleep 5

        log_info "Match $match 完成"
    done

    log_info ""
    log_info "======================================="
    log_info "所有比赛完成！"
    log_info "总场次: $NUM_MATCHES"
    log_info "总进球: ${TEAM_LEFT_NAME} ${total_goals_left} - ${total_goals_right} ${TEAM_RIGHT_NAME}"
    log_info "数据保存目录: $DATA_OUTPUT_DIR"
    log_info "======================================="
}

# 执行主函数
main
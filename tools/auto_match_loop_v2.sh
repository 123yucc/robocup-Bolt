#!/bin/bash
#
# Bolt Team - 自动化循环比赛系统（修复版）
# 兼容 rcssserver 19.0.0 新参数格式
#

set -e

# ================= 配置区域 =================
NUM_MATCHES=5                     # 比赛场次（先测试5场）
SERVER_PORT=6000                  # Server端口
PROJECT_DIR="/home/linna/Cyrus2DBase"
BUILD_DIR="$PROJECT_DIR/build/bin"   # 球员二进制目录
TEAM_LEFT_NAME="BoltRL"
TEAM_RIGHT_NAME="BoltBase"
DATA_OUTPUT_DIR="$PROJECT_DIR/data/matches"
LOG_DIR="$PROJECT_DIR/logs"
MATCH_DURATION=600                # 比赛时长（秒）约10分钟
TOOLS_DIR="$PROJECT_DIR/tools"
# ============================================

# 创建目录
mkdir -p "$DATA_OUTPUT_DIR"
mkdir -p "$LOG_DIR"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

# ================= 进程管理 =================

cleanup_all() {
    log_step "清理所有进程..."
    pkill -9 -f "rcssserver" 2>/dev/null || true
    pkill -9 -f "sample_player" 2>/dev/null || true
    pkill -9 -f "sample_coach" 2>/dev/null || true
    pkill -9 -f "rcssmonitor" 2>/dev/null || true
    sleep 3
    log_info "清理完成"
}

# ================= Server 管理 =================

start_server() {
    local match_num=$1
    local rcg_file="$LOG_DIR/match_${match_num}.rcg"
    local rcl_file="$LOG_DIR/match_${match_num}.rcl"

    log_step "启动 Server (Match $match_num)..."

    # rcssserver 19.0.0 新参数格式
    # 注意：端口和日志路径使用 namespace::option=value 格式
    rcssserver \
        server::port=$SERVER_PORT \
        server::coach_port=$((SERVER_PORT + 1)) \
        server::olcoach_port=$((SERVER_PORT + 2)) \
        server::game_logging=on \
        server::text_logging=on \
        server::game_log_fixed=on \
        server::text_log_fixed=on \
        server::game_log_fixed_name="$rcg_file" \
        server::text_log_fixed_name="$rcl_file" \
        server::game_log_dated=off \
        server::text_log_dated=off \
        server::synch_mode=on \
        server::synch_offset=60 \
        > "$LOG_DIR/server_${match_num}.log" 2>&1 &

    SERVER_PID=$!
    sleep 3

    # 检查server是否启动成功
    if ! ps -p $SERVER_PID > /dev/null 2>&1; then
        log_error "Server启动失败！查看日志: $LOG_DIR/server_${match_num}.log"
        cat "$LOG_DIR/server_${match_num}.log" | head -30
        return 1
    fi

    log_info "Server已启动 (PID: $SERVER_PID)"
    return 0
}

# ================= 球队管理 =================

start_team() {
    local team_name=$1

    log_step "启动球队 (${team_name})..."

    # 必须从 build/bin 目录运行（formation文件在该目录）
    cd "$BUILD_DIR"

    # 启动11个球员（使用正确的参数格式）
    # 第一个球员是守门员，使用 -g 参数
    ./sample_player \
        --player-config player.conf \
        --config_dir formations-dt \
        -h localhost \
        -p $SERVER_PORT \
        -t "$team_name" \
        -g \
        > "$LOG_DIR/${team_name}_player_1.log" 2>&1 &
    sleep 1

    # 启动其余球员（2-11号，非守门员）
    for i in {2..11}; do
        ./sample_player \
            --player-config player.conf \
            --config_dir formations-dt \
            -h localhost \
            -p $SERVER_PORT \
            -t "$team_name" \
            > "$LOG_DIR/${team_name}_player_${i}.log" 2>&1 &
        sleep 0.3
    done

    cd - > /dev/null

    # 等待球员连接
    sleep 3

    # 检查进程数
    local count=$(pgrep -c sample_player 2>/dev/null || echo 0)
    log_info "球队已启动 (${count}个球员进程)"

    return 0
}

# ================= 自动开球 =================

send_kickoff() {
    log_step "发送自动开球指令..."

    # 使用 sample_coach 发送开球指令
    cd "$BUILD_DIR"

    # 启动 sample_coach（它会自动发送开球指令）
    # coach_port 默认是 server_port + 2
    ./sample_coach \
        --coach-config coach.conf \
        -h localhost \
        -p $((SERVER_PORT + 2)) \
        -t "$TEAM_LEFT_NAME" \
        > "$LOG_DIR/coach.log" 2>&1 &

    COACH_PID=$!
    sleep 3

    cd - > /dev/null

    log_info "Coach已启动，比赛将自动开始"
}

# ================= 等待比赛结束 =================

wait_for_match_end() {
    local match_num=$1
    local elapsed=0
    local max_wait=$MATCH_DURATION

    log_step "等待比赛结束..."

    while [ $elapsed -lt $max_wait ]; do
        # 检查server进程
        if ! ps -p $SERVER_PID > /dev/null 2>&1; then
            log_info "Server进程已退出，比赛结束"
            break
        fi

        # 每60秒显示一次进度
        if [ $((elapsed % 60)) -eq 0 ]; then
            log_info "比赛进行中... ${elapsed}秒 / ${max_wait}秒"
        fi

        sleep 10
        elapsed=$((elapsed + 10))
    done

    # 强制结束比赛
    if [ $elapsed -ge $max_wait ]; then
        log_warn "比赛超时，强制结束"
    fi
}

# ================= 数据收集 =================

collect_data() {
    local match_num=$1
    local rcg_file="$LOG_DIR/match_${match_num}.rcg"
    local csv_file="$DATA_OUTPUT_DIR/match_${match_num}_data.csv"

    log_step "收集比赛数据..."

    if [ ! -f "$rcg_file" ]; then
        log_warn "未找到日志文件: $rcg_file"
        return 1
    fi

    # 调用数据提取脚本
    python3 "$TOOLS_DIR/extract_match_data.py" \
        --rcg "$rcg_file" \
        --output "$csv_file" \
        --match-num "$match_num" || log_warn "数据提取失败"

    if [ -f "$csv_file" ]; then
        log_info "数据已保存: $csv_file ($(wc -l < "$csv_file") 行)"
    fi
}

# ================= 结果统计 =================

get_result() {
    local match_num=$1
    local rcl_file="$LOG_DIR/match_${match_num}.rcl"

    local goals_left=0
    local goals_right=0

    if [ -f "$rcl_file" ]; then
        goals_left=$(grep -c "goal_l_" "$rcl_file" 2>/dev/null || echo 0)
        goals_right=$(grep -c "goal_r_" "$rcl_file" 2>/dev/null || echo 0)
    fi

    log_info "Match $match_num 结果: ${TEAM_LEFT_NAME} ${goals_left} - ${goals_right} ${TEAM_RIGHT_NAME}"
    return 0
}

# ================= 主循环 =================

main() {
    log_info "======================================="
    log_info "Bolt 自动化比赛循环系统"
    log_info "======================================="
    log_info "总场次: $NUM_MATCHES"
    log_info "左侧球队: $TEAM_LEFT_NAME"
    log_info "右侧球队: $TEAM_RIGHT_NAME"
    log_info "======================================="

    # 检查依赖
    if ! command -v rcssserver &> /dev/null; then
        log_error "rcssserver 未安装！"
        exit 1
    fi

    if [ ! -f "$BUILD_DIR/sample_player" ]; then
        log_error "sample_player 未编译！路径: $BUILD_DIR/sample_player"
        exit 1
    fi

    # 初始清理
    cleanup_all

    # 比赛循环
    for match in $(seq 1 $NUM_MATCHES); do
        log_info ""
        log_info "==================================== Match $match / $NUM_MATCHES ================================="

        # 1. 启动 Server
        if ! start_server $match; then
            log_error "Match $match 启动失败，跳过"
            cleanup_all
            continue
        fi

        # 2. 启动左侧球队
        start_team "$TEAM_LEFT_NAME" true

        # 3. 启动右侧球队
        start_team "$TEAM_RIGHT_NAME" false

        # 4. 自动开球
        send_kickoff

        # 5. 等待比赛结束
        wait_for_match_end $match

        # 6. 收集数据
        collect_data $match

        # 7. 统计结果
        get_result $match

        # 8. 清理进程
        cleanup_all

        # 短暂休息
        log_info "准备下一场比赛..."
        sleep 10
    done

    # ================= 最终统计 =================
    log_info ""
    log_info "======================================="
    log_info "所有比赛完成！"
    log_info "数据保存目录: $DATA_OUTPUT_DIR"
    log_info "======================================="

    # 合并所有CSV文件
    if ls "$DATA_OUTPUT_DIR"/match_*_data.csv 1> /dev/null 2>&1; then
        log_step "合并所有数据..."
        merged_csv="$DATA_OUTPUT_DIR/all_matches_data.csv"

        head -n 1 "$DATA_OUTPUT_DIR/match_1_data.csv" > "$merged_csv" 2>/dev/null || true
        for f in "$DATA_OUTPUT_DIR"/match_*_data.csv; do
            tail -n +2 "$f" >> "$merged_csv" 2>/dev/null || true
        done

        log_info "合并完成: $merged_csv ($(wc -l < "$merged_csv") 行)"
    fi
}

# 执行
main
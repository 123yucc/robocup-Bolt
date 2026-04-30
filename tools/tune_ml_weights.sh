#!/bin/bash
# ML权重系统化调优脚本
# 用于自动化测试不同ML权重组合的性能

set -e

# 配置
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
RESULTS_DIR="${PROJECT_DIR}/ml_tuning_results"
RESULTS_FILE="${RESULTS_DIR}/tuning_results.csv"
BASELINE_TEAM="agent2d"
NUM_GAMES_PER_CONFIG=5

# 创建结果目录
mkdir -p "${RESULTS_DIR}"

# 初始化结果文件
if [ ! -f "${RESULTS_FILE}" ]; then
    echo "LAMBDA_SHOT,LAMBDA_UNMARK,LAMBDA_PASS,LAMBDA_DEFENSE,wins,draws,losses,goals_for,goals_against,goal_diff,score" > "${RESULTS_FILE}"
fi

# 权重候选值
LAMBDA_SHOT_VALUES=(25000 50000 75000 100000 150000 200000)
LAMBDA_UNMARK_VALUES=(5.0 10.0 15.0 20.0 30.0)
LAMBDA_PASS_VALUES=(25.0 50.0 75.0 100.0)
LAMBDA_DEFENSE_VALUES=(15.0 30.0 45.0 60.0)

# 默认权重（基线）
DEFAULT_SHOT=50000
DEFAULT_UNMARK=10.0
DEFAULT_PASS=50.0
DEFAULT_DEFENSE=30.0

# 日志函数
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"
}

# 修改权重函数
update_weight() {
    local file=$1
    local var_name=$2
    local new_value=$3

    # 使用sed替换权重值
    sed -i "s/static const double ${var_name} = [0-9.e+]*;/static const double ${var_name} = ${new_value};/" "${file}"
}

# 编译项目
compile_project() {
    log "编译项目..."
    cd "${BUILD_DIR}"
    make -j$(nproc) > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        log "编译失败"
        return 1
    fi
    log "编译成功"
    return 0
}

# 运行对战
run_match() {
    local game_num=$1
    log "运行第 ${game_num} 场比赛..."

    # 这里需要根据实际的测试脚本调整
    # 假设有一个测试脚本可以运行对战并返回结果
    # 返回格式: "wins draws losses goals_for goals_against"

    # 示例：使用rcssserver运行比赛
    # 实际实现需要根据项目的测试框架调整

    # 临时返回模拟结果（实际使用时需要替换）
    echo "1 0 0 2 1"
}

# 解析比赛结果
parse_results() {
    local results=$1
    local wins=$(echo $results | cut -d' ' -f1)
    local draws=$(echo $results | cut -d' ' -f2)
    local losses=$(echo $results | cut -d' ' -f3)
    local goals_for=$(echo $results | cut -d' ' -f4)
    local goals_against=$(echo $results | cut -d' ' -f5)

    echo "${wins} ${draws} ${losses} ${goals_for} ${goals_against}"
}

# 计算综合得分
calculate_score() {
    local wins=$1
    local draws=$2
    local losses=$3
    local goal_diff=$4

    # score = 3*wins + draws - 2*losses + goal_diff
    echo "scale=2; 3*${wins} + ${draws} - 2*${losses} + ${goal_diff}" | bc
}

# 测试单个权重配置
test_configuration() {
    local shot=$1
    local unmark=$2
    local pass=$3
    local defense=$4

    log "测试配置: SHOT=${shot}, UNMARK=${unmark}, PASS=${pass}, DEFENSE=${defense}"

    # 更新权重
    update_weight "${PROJECT_DIR}/src/player/sample_field_evaluator.cpp" "LAMBDA_SHOT" "${shot}"
    update_weight "${PROJECT_DIR}/src/player/sample_field_evaluator.cpp" "LAMBDA_PASS" "${pass}"
    update_weight "${PROJECT_DIR}/src/player/bhv_unmark.cpp" "LAMBDA_UNMARK" "${unmark}"
    # LAMBDA_DEFENSE 需要根据实际文件位置调整

    # 编译
    if ! compile_project; then
        log "跳过此配置（编译失败）"
        return 1
    fi

    # 运行多场比赛
    local total_wins=0
    local total_draws=0
    local total_losses=0
    local total_goals_for=0
    local total_goals_against=0

    for i in $(seq 1 ${NUM_GAMES_PER_CONFIG}); do
        local result=$(run_match $i)
        local parsed=$(parse_results "${result}")

        local wins=$(echo $parsed | cut -d' ' -f1)
        local draws=$(echo $parsed | cut -d' ' -f2)
        local losses=$(echo $parsed | cut -d' ' -f3)
        local goals_for=$(echo $parsed | cut -d' ' -f4)
        local goals_against=$(echo $parsed | cut -d' ' -f5)

        total_wins=$((total_wins + wins))
        total_draws=$((total_draws + draws))
        total_losses=$((total_losses + losses))
        total_goals_for=$((total_goals_for + goals_for))
        total_goals_against=$((total_goals_against + goals_against))
    done

    # 计算统计数据
    local goal_diff=$((total_goals_for - total_goals_against))
    local score=$(calculate_score ${total_wins} ${total_draws} ${total_losses} ${goal_diff})

    # 保存结果
    echo "${shot},${unmark},${pass},${defense},${total_wins},${total_draws},${total_losses},${total_goals_for},${total_goals_against},${goal_diff},${score}" >> "${RESULTS_FILE}"

    log "结果: W=${total_wins} D=${total_draws} L=${total_losses} GF=${total_goals_for} GA=${total_goals_against} Score=${score}"
}

# 主函数
main() {
    log "开始ML权重调优实验"
    log "结果将保存到: ${RESULTS_FILE}"

    # 测试策略：单变量调优
    # 每次只改变一个权重，其他保持默认值

    log "=== 调优 LAMBDA_SHOT ==="
    for shot in "${LAMBDA_SHOT_VALUES[@]}"; do
        test_configuration ${shot} ${DEFAULT_UNMARK} ${DEFAULT_PASS} ${DEFAULT_DEFENSE}
    done

    log "=== 调优 LAMBDA_UNMARK ==="
    for unmark in "${LAMBDA_UNMARK_VALUES[@]}"; do
        test_configuration ${DEFAULT_SHOT} ${unmark} ${DEFAULT_PASS} ${DEFAULT_DEFENSE}
    done

    log "=== 调优 LAMBDA_PASS ==="
    for pass in "${LAMBDA_PASS_VALUES[@]}"; do
        test_configuration ${DEFAULT_SHOT} ${DEFAULT_UNMARK} ${pass} ${DEFAULT_DEFENSE}
    done

    log "=== 调优 LAMBDA_DEFENSE ==="
    for defense in "${LAMBDA_DEFENSE_VALUES[@]}"; do
        test_configuration ${DEFAULT_SHOT} ${DEFAULT_UNMARK} ${DEFAULT_PASS} ${defense}
    done

    log "调优完成！"
    log "结果已保存到: ${RESULTS_FILE}"
    log "使用以下命令分析结果:"
    log "  python3 tools/analyze_tuning_results.py ${RESULTS_FILE}"
}

# 运行主函数
main "$@"

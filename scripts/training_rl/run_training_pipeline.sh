#!/bin/bash
#
# Bolt Team - 一键训练管道
#
# 功能: 自动完成 数据收集 → 模型训练 → 权重转换
#

set -e  # 遇错即停

# 配置
NUM_MATCHES=50           # 数据收集场次
TRAINING_EPOCHS=100      # 训练轮数
BATCH_SIZE=64            # 批次大小
LEARNING_RATE=0.0001     # 学习率
STATE_DIM=350            # 状态维度
NUM_ACTIONS=4            # 动作数量

# 目录
PROJECT_DIR="/home/linna/Cyrus2DBase"
DATA_DIR="$PROJECT_DIR/data/matches"
WEIGHTS_DIR="$PROJECT_DIR/weights"
LOGS_DIR="$PROJECT_DIR/logs"

# 创建目录
mkdir -p "$DATA_DIR"
mkdir -p "$WEIGHTS_DIR"
mkdir -p "$LOGS_DIR"

echo "=========================================="
echo "Bolt Team - RL Training Pipeline"
echo "=========================================="
echo "比赛场次: $NUM_MATCHES"
echo "训练轮数: $TRAINING_EPOCHS"
echo "=========================================="

# ================= Phase 1: 数据收集 =================
echo ""
echo "Phase 1: 自动化比赛数据收集"
echo "------------------------------------------"

if [ -f "$DATA_DIR/all_matches_data.csv" ]; then
    data_lines=$(wc -l < "$DATA_DIR/all_matches_data.csv")
    echo "已有数据文件: $data_lines 行"

    read -p "是否重新收集数据? (y/n): " choice
    if [ "$choice" == "y" ]; then
        echo "开始数据收集..."
        cd "$PROJECT_DIR"
        ./tools/auto_match_loop_v2.sh
    else
        echo "使用现有数据"
    fi
else
    echo "开始数据收集（首次运行）..."
    cd "$PROJECT_DIR"
    ./tools/auto_match_loop_v2.sh
fi

# 检查数据
if [ ! -f "$DATA_DIR/all_matches_data.csv" ]; then
    echo "错误: 数据文件不存在！"
    exit 1
fi

data_size=$(wc -l < "$DATA_DIR/all_matches_data.csv")
echo "数据收集完成: $data_size 行"

# ================= Phase 2: 模型训练 =================
echo ""
echo "Phase 2: RL模型训练"
echo "------------------------------------------"

cd "$PROJECT_DIR"

python3 scripts/training_rl/train_rl_agent.py \
    --data "$DATA_DIR/all_matches_data.csv" \
    --epochs $TRAINING_EPOCHS \
    --batch_size $BATCH_SIZE \
    --lr $LEARNING_RATE \
    --output "$WEIGHTS_DIR" \
    --cppdnn "$WEIGHTS_DIR/rl_value_weights.txt"

# 检查训练结果
if [ ! -f "$WEIGHTS_DIR/value_network.h5" ]; then
    echo "错误: 训练失败！"
    exit 1
fi

echo "训练完成!"

# ================= Phase 3: 权重转换 =================
echo ""
echo "Phase 3: 权重格式转换"
echo "------------------------------------------"

# 转换价值网络
if [ -f "$WEIGHTS_DIR/rl_value_weights.txt" ]; then
    echo "CppDNN权重已生成: $WEIGHTS_DIR/rl_value_weights.txt"
else
    python3 scripts/training_rl/convert_weights_to_cppdnn.py \
        --model "$WEIGHTS_DIR/value_network.h5" \
        --output "$WEIGHTS_DIR/rl_value.txt" \
        --type value
fi

# 转换策略网络（如果存在）
if [ -f "$WEIGHTS_DIR/policy_network.h5" ]; then
    python3 scripts/training_rl/convert_weights_to_cppdnn.py \
        --model "$WEIGHTS_DIR/policy_network.h5" \
        --output "$WEIGHTS_DIR/rl_policy.txt" \
        --type policy
fi

echo "权重转换完成!"

# ================= Phase 4: 编译C++ =================
echo ""
echo "Phase 4: 编译C++程序"
echo "------------------------------------------"

cd "$PROJECT_DIR/build"
make -j$(nproc)

echo "编译完成!"

# ================= 完成 =================
echo ""
echo "=========================================="
echo "训练管道完成!"
echo "=========================================="
echo "权重文件:"
ls -lh "$WEIGHTS_DIR"/*.txt 2>/dev/null || echo "  无权重文件"
echo ""
echo "下一步: 运行测试比赛验证RL增强效果"
echo "  ./tools/auto_match_loop_v2.sh"
echo ""
echo "对比指标:"
echo "  - 进球数"
echo "  - 控球率"
echo "  - 传球成功率"
echo "=========================================="
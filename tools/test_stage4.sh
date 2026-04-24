#!/bin/bash

# 阶段4优化效果测试脚本
# BoltV4 (阶段2+3+4) vs BoltV3 (阶段2+3)

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  阶段4优化效果测试                                         ║${NC}"
echo -e "${CYAN}║  BoltV4 (阶段2+3+4) vs BoltV3 (阶段2+3)                    ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
echo

# 获取当前提交
CURRENT_COMMIT=$(git rev-parse HEAD)
STAGE3_COMMIT="ddca4a9"  # 阶段3的提交

# 1. 准备BoltV3（阶段2+3）
echo -e "${YELLOW}[1/5] 准备BoltV3（阶段2+3）...${NC}"
git stash
git checkout $STAGE3_COMMIT
cd build && cmake .. > /dev/null 2>&1 && make -j$(nproc) > /dev/null 2>&1
cd ..
mkdir -p build_v3
cp -r build/* build_v3/
git checkout -
git stash pop || true
echo -e "${GREEN}✓ BoltV3 准备完成${NC}"
echo

# 2. 启动 rcssserver
echo -e "${YELLOW}[2/5] 启动 rcssserver...${NC}"
pkill -9 rcssserver || true
sleep 1
rcssserver server::auto_mode=true server::synch_mode=true server::half_time=300 server::nr_normal_halfs=2 > /dev/null 2>&1 &
SERVER_PID=$!
sleep 2
echo -e "${GREEN}✓ Server 已启动 (PID: $SERVER_PID)${NC}"
echo

# 3. 启动 BoltV4 (当前版本，左侧)
echo -e "${YELLOW}[3/5] 启动 BoltV4 (阶段2+3+4优化版，左侧)...${NC}"
cd build/bin
./start.sh -t BoltV4 > /dev/null 2>&1 &
BOLT_V4_PID=$!
cd ../..
sleep 3
echo -e "${GREEN}✓ BoltV4 已启动 (PID: $BOLT_V4_PID)${NC}"
echo

# 4. 启动 BoltV3 (阶段2+3，右侧)
echo -e "${YELLOW}[4/5] 启动 BoltV3 (阶段2+3，右侧)...${NC}"
cd build_v3/bin
./start.sh -t BoltV3 > /dev/null 2>&1 &
BOLT_V3_PID=$!
cd ../..
sleep 3
echo -e "${GREEN}✓ BoltV3 已启动 (PID: $BOLT_V3_PID)${NC}"
echo

# 5. 启动 rcssmonitor
echo -e "${YELLOW}[5/5] 启动 rcssmonitor...${NC}"
rcssmonitor > /dev/null 2>&1 &
MONITOR_PID=$!
sleep 2
echo -e "${GREEN}✓ rcssmonitor 已启动 (PID: $MONITOR_PID)${NC}"
echo

echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║  测试环境已就绪！                                          ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
echo
echo -e "${BLUE}对战配置:${NC}"
echo -e "  ${GREEN}左侧 (BoltV4):${NC} 阶段2+3+4优化"
echo -e "    ✓ 搜索深度: 6步"
echo -e "    ✓ 评估次数: 1000次"
echo -e "    ✓ 射门采样: 50点 (新增)"
echo -e "    ✓ 直传角度: 36方向 (新增)"
echo -e "    ✓ 引导传球: 20方向 (新增)"
echo -e "    ✓ 铲球阈值: 0.4/0.65 (新增)"
echo -e "    ✓ 智能守门员出击"
echo
echo -e "  ${YELLOW}右侧 (BoltV3):${NC} 阶段2+3优化"
echo -e "    ✓ 搜索深度: 6步"
echo -e "    ✓ 评估次数: 1000次"
echo -e "    ✓ 射门采样: 32点"
echo -e "    ✓ 直传角度: 24方向"
echo -e "    ✓ 引导传球: 14方向"
echo -e "    ✓ 铲球阈值: 0.5/0.8"
echo -e "    ✓ 智能守门员出击"
echo
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}请在 rcssmonitor 中按 Ctrl+K 开球！${NC}"
echo -e "${CYAN}════════════════════════════════════════════════════════════${NC}"
echo
echo -e "${YELLOW}比赛结束后按 Ctrl+C 查看结果${NC}"
echo

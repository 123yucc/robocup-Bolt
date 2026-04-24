#!/bin/bash

# 阶段6测试脚本：BoltV6 vs BoltV5
# 测试跑位接球优化效果

echo "=========================================="
echo "阶段6测试：BoltV6 vs BoltV5"
echo "优化内容：跑位接球（采样密度+传球目标点+动态体力）"
echo "=========================================="

# 清理所有进程
pkill -9 rcssserver
pkill -9 sample_player
pkill -9 rcssmonitor
sleep 2

# 启动服务器
echo "启动rcssserver..."
rcssserver server::auto_mode=true server::synch_mode=true server::half_time=300 server::nr_normal_halfs=2 &
sleep 3

# 启动监视器
echo "启动rcssmonitor..."
rcssmonitor &
sleep 2

# 启动BoltV6（左侧）
echo "启动BoltV6（左侧）..."
cd /home/linna/robocup-Bolt/build/bin
./start.sh -t BoltV6 -f formations-dt &
sleep 3

# 启动BoltV5（右侧）
echo "启动BoltV5（右侧）..."
cd /home/linna/robocup-Bolt-stage5/build/bin
./start.sh -t BoltV5 -f formations-dt &
sleep 3

echo ""
echo "=========================================="
echo "测试环境已启动！"
echo "左侧：BoltV6（跑位接球优化版）"
echo "右侧：BoltV5（防守拦截优化版）"
echo "=========================================="
echo ""
echo "请在rcssmonitor中点击开球按钮开始比赛"
echo "比赛时长：2个半场，每半场300周期"
echo ""

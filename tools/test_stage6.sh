#!/bin/bash

# 阶段6测试：BoltV6 vs BoltV5
# 优化内容：跑位接球优化（采样密度、传球目标点、动态体力阈值）

echo "=========================================="
echo "阶段6测试：BoltV6 vs BoltV5"
echo "优化内容："
echo "  - 跑位采样密度提升（近距离20°→10°，远距离10°→5°）"
echo "  - 传球目标点扩展（5点→80点）"
echo "  - 动态体力阈值（比赛后期降低20%）"
echo "=========================================="

# 启动服务器
rcssserver server::auto_mode=true server::synch_mode=true server::half_time=300 server::nr_normal_halfs=2 &
SERVER_PID=$!
sleep 2

# 启动BoltV6（左侧）
cd /home/linna/robocup-Bolt
./start.sh -t Bolt &
BOLT_V6_PID=$!
sleep 2

# 启动BoltV5（右侧）
cd /home/linna/robocup-Bolt-stage5
./start.sh -t BoltV5 &
BOLT_V5_PID=$!
sleep 2

# 启动监视器
rcssmonitor &
MONITOR_PID=$!

echo ""
echo "测试已启动："
echo "  - 左侧（Bolt）：BoltV6"
echo "  - 右侧（BoltV5）：BoltV5"
echo ""
echo "按Ctrl+C结束测试..."

wait $SERVER_PID

kill $BOLT_V6_PID $BOLT_V5_PID $MONITOR_PID 2>/dev/null

echo ""
echo "测试结束"

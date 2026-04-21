#!/usr/bin/env python3
"""
Bolt Team - 自动开球 Coach 脚本

关键功能：替代手动 Ctrl+K 操作，自动发送 (play_on) 指令开始比赛

原理：
- RoboCup server 默认需要 monitor 发送 Ctrl+K 开球
- Coach 连接后可以发送指令控制比赛
- 通过发送 (play_on) 指令自动开始比赛

使用：
python3 auto_match_coach.py --port 6000 --team-left BoltRL --team-right BoltBase
"""

import socket
import argparse
import time
import sys


class AutoKickoffCoach:
    """自动开球 Coach"""

    def __init__(self, port, team_left, team_right):
        self.port = port
        self.team_left = team_left
        self.team_right = team_right
        self.socket = None
        self.server_host = 'localhost'

    def connect(self):
        """连接到 server"""
        print(f"连接到 server ({self.server_host}:{self.port})...")

        # UDP socket
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.settimeout(5.0)

        # 发送初始化消息
        init_msg = f"(init {self.team_left} (version 15))"
        self._send_message(init_msg)

        # 等待响应
        response = self._receive_message()
        if response:
            print(f"Server响应: {response}")

        return True

    def _send_message(self, msg):
        """发送UDP消息"""
        self.socket.sendto(msg.encode(), (self.server_host, self.port))

    def _receive_message(self):
        """接收UDP消息"""
        try:
            data, addr = self.socket.recvfrom(4096)
            return data.decode()
        except socket.timeout:
            return None

    def wait_for_teams(self):
        """等待两队连接"""
        print("等待两队连接...")
        max_wait = 30  # 最大等待30秒
        start_time = time.time()

        while time.time() - start_time < max_wait:
            # 发送检查消息
            check_msg = "(check)"
            self._send_message(check_msg)

            response = self._receive_message()
            if response:
                # 检查两队是否都已连接
                if f'("{self.team_left}"' in response and f'"{self.team_right}"' in response:
                    print(f"两队已连接: {self.team_left} vs {self.team_right}")
                    return True

            time.sleep(1)

        print("等待超时，两队可能未完全连接")
        return False

    def send_kickoff(self):
        """发送开球指令"""
        print("发送开球指令 (play_on)...")

        # 关键：发送 (play_on) 指令开始比赛
        # 这相当于手动按 Ctrl+K
        kickoff_msg = "(play_on)"
        self._send_message(kickoff_msg)

        # 等待确认
        response = self._receive_message()
        if response:
            print(f"开球确认: {response}")

        print("比赛已开始！")

    def monitor_match(self, duration_seconds=600):
        """监控比赛进程"""
        print(f"监控比赛进程（预计{duration_seconds}秒）...")

        start_time = time.time()

        while time.time() - start_time < duration_seconds:
            # 定期发送心跳消息
            heartbeat = "(eye_on)"
            self._send_message(heartbeat)

            # 接收比赛状态
            response = self._receive_message()
            if response:
                # 检查是否比赛结束
                if 'time_over' in response or 'game_over' in response:
                    print("比赛已结束！")
                    break

                # 显示进度
                elapsed = int(time.time() - start_time)
                if elapsed % 60 == 0:  # 每分钟显示一次
                    print(f"比赛进行中: {elapsed}秒")

            time.sleep(5)

    def disconnect(self):
        """断开连接"""
        print("断开连接...")
        if self.socket:
            self.socket.close()


def main():
    parser = argparse.ArgumentParser(description='自动开球Coach')
    parser.add_argument('--port', type=int, default=6000, help='Server端口')
    parser.add_argument('--team-left', required=True, help='左侧球队名')
    parser.add_argument('--team-right', required=True, help='右侧球队名')
    parser.add_argument('--duration', type=int, default=600, help='比赛时长（秒）')

    args = parser.parse_args()

    coach = AutoKickoffCoach(args.port, args.team_left, args.team_right)

    try:
        # 1. 连接
        if not coach.connect():
            print("连接失败！")
            sys.exit(1)

        # 2. 等待两队连接
        if not coach.wait_for_teams():
            print("等待超时，继续尝试开球...")

        # 3. 发送开球指令（关键！）
        coach.send_kickoff()

        # 4. 监控比赛
        coach.monitor_match(args.duration)

    except KeyboardInterrupt:
        print("手动中断")
    except Exception as e:
        print(f"错误: {e}")
    finally:
        coach.disconnect()


if __name__ == '__main__':
    main()
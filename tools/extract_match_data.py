#!/usr/bin/env python3
"""
Bolt Team - 比赛数据提取脚本
从 .rcg (RoboCup Game) 日志文件提取训练数据

功能：
1. 解析 .rcg 文件格式
2. 提取状态特征、动作、奖励
3. 输出 CSV 格式供 RL 训练使用
"""

import argparse
import os
import re
import csv
import gzip
import struct
from datetime import datetime
import numpy as np


class RCGParser:
    """RoboCup .rcg 日志文件解析器"""

    def __init__(self, rcg_path):
        self.rcg_path = rcg_path
        self.frames = []
        self.match_info = {}

    def parse(self):
        """解析 .rcg 文件"""
        try:
            # 尝试 gzip 解压（新版日志）
            if self.rcg_path.endswith('.gz'):
                with gzip.open(self.rcg_path, 'rt') as f:
                    self._parse_text(f)
            else:
                # 先尝试文本格式
                try:
                    with open(self.rcg_path, 'r') as f:
                        self._parse_text(f)
                except:
                    # 二进制格式（旧版日志）
                    with open(self.rcg_path, 'rb') as f:
                        self._parse_binary(f)

            return True
        except Exception as e:
            print(f"Error parsing {self.rcg_path}: {e}")
            return False

    def _parse_text(self, f):
        """解析文本格式的 .rcg 文件"""
        for line in f:
            line = line.strip()
            if not line:
                continue

            # 解析比赛信息
            if line.startswith('@'):
                self._parse_header(line)
            elif line.startswith('('):
                self._parse_frame(line)

    def _parse_header(self, line):
        """解析文件头信息"""
        parts = line.split()
        if len(parts) > 1:
            if 'team_left' in line:
                match = re.search(r'team_left_(\w+)', line)
                if match:
                    self.match_info['team_left'] = match.group(1)
            if 'team_right' in line:
                match = re.search(r'team_right_(\w+)', line)
                if match:
                    self.match_info['team_right'] = match.group(1)

    def _parse_frame(self, line):
        """解析每一帧数据"""
        frame = {}

        # 解析周期数
        match = re.search(r'(\d+)\s+\(', line)
        if match:
            frame['cycle'] = int(match.group(1))

        # 解析球位置
        ball_match = re.search(r'\(\s*b\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)', line)
        if ball_match:
            frame['ball_x'] = float(ball_match.group(1))
            frame['ball_y'] = float(ball_match.group(2))
            frame['ball_vx'] = float(ball_match.group(3))
            frame['ball_vy'] = float(ball_match.group(4))

        # 解析球员位置
        # 格式: (p "team" unum x y vx vy body_angle neck_angle)
        players = re.findall(r'\(p\s+"([^"]+)"\s+(\d+)\s+([-\d.]+)\s+([-\d.]+)', line)
        if players:
            frame['players'] = []
            for player in players:
                frame['players'].append({
                    'team': player[0],
                    'unum': int(player[1]),
                    'x': float(player[2]),
                    'y': float(player[3])
                })

        # 解析比分
        score_match = re.search(r'\(\s*score\s+(\d+)\s+(\d+)\)', line)
        if score_match:
            frame['score_left'] = int(score_match.group(1))
            frame['score_right'] = int(score_match.group(2))

        # 解析游戏模式
        if 'play_on' in line:
            frame['game_mode'] = 'play_on'
        elif 'goal_l_' in line:
            frame['game_mode'] = 'goal_left'
            frame['goal_scored'] = True
        elif 'goal_r_' in line:
            frame['game_mode'] = 'goal_right'
            frame['goal_conceded'] = True

        self.frames.append(frame)

    def _parse_binary(self, f):
        """解析二进制格式的 .rcg 文件（旧版）"""
        # 二进制格式较复杂，这里简化处理
        # 实际项目中建议使用 libsoccer 库
        pass


class MatchDataExtractor:
    """从比赛日志提取RL训练数据"""

    def __init__(self, parser, match_num=0):
        self.parser = parser
        self.match_num = match_num
        self.training_data = []

    def extract(self):
        """提取训练数据"""
        frames = self.parser.frames

        if len(frames) < 100:
            print(f"警告：比赛数据太少（{len(frames)}帧）")
            return []

        # 遍历每一帧，提取状态-动作-奖励序列
        for i in range(len(frames) - 1):
            current_frame = frames[i]
            next_frame = frames[i + 1]

            # 计算奖励
            reward = self._calculate_reward(current_frame, next_frame)

            # 提取特征
            features = self._extract_features(current_frame)

            # 提取动作（基于球位置变化推断）
            action = self._infer_action(current_frame, next_frame)

            # 构建训练样本
            sample = {
                'match_num': self.match_num,
                'cycle': current_frame.get('cycle', i),
                'features': features,
                'action': action,
                'reward': reward,
                'ball_x': current_frame.get('ball_x', 0),
                'ball_y': current_frame.get('ball_y', 0),
                'score_left': current_frame.get('score_left', 0),
                'score_right': current_frame.get('score_right', 0),
                'game_mode': current_frame.get('game_mode', 'unknown')
            }

            self.training_data.append(sample)

        return self.training_data

    def _extract_features(self, frame):
        """提取350维特征"""
        features = []

        # 球位置和速度（4维）
        ball_x = frame.get('ball_x', 0)
        ball_y = frame.get('ball_y', 0)
        ball_vx = frame.get('ball_vx', 0)
        ball_vy = frame.get('ball_vy', 0)

        # 归一化
        features.append((ball_x + 52.5) / 105.0)  # x: [-52.5, 52.5] -> [0, 1]
        features.append((ball_y + 34.0) / 68.0)   # y: [-34, 34] -> [0, 1]
        features.append(ball_vx / 30.0)           # vx 归一化
        features.append(ball_vy / 30.0)           # vy 归一化

        # 球员位置（假设22个球员，每人位置2维 = 44维）
        players = frame.get('players', [])
        for team in ['left', 'right']:
            team_players = [p for p in players if p['team'] == self.parser.match_info.get(f'team_{team}', '')]
            for unum in range(1, 12):
                player = next((p for p in team_players if p['unum'] == unum), None)
                if player:
                    features.append((player['x'] + 52.5) / 105.0)
                    features.append((player['y'] + 34.0) / 68.0)
                else:
                    features.append(0.5)  # 默认中心位置
                    features.append(0.5)

        # 比分（2维）
        features.append(frame.get('score_left', 0) / 10.0)
        features.append(frame.get('score_right', 0) / 10.0)

        # 游戏模式（编码）
        game_mode = frame.get('game_mode', 'play_on')
        mode_encoding = {
            'play_on': [1, 0, 0, 0],
            'goal_left': [0, 1, 0, 0],
            'goal_right': [0, 0, 1, 0],
            'unknown': [0, 0, 0, 1]
        }
        features.extend(mode_encoding.get(game_mode, [0, 0, 0, 1]))

        # 扩展特征（剩余维度填充）
        # 实际项目中应该添加更多特征：
        # - 球员体力
        # - 距球距离
        # - 角度信息
        # - Voronoi区域
        # - 传球威胁度
        # - 射门机会评估
        # ...

        # 确保特征维度一致
        target_dim = 350
        while len(features) < target_dim:
            features.append(0.0)

        return features[:target_dim]

    def _infer_action(self, current_frame, next_frame):
        """从帧变化推断动作"""
        ball_x_current = current_frame.get('ball_x', 0)
        ball_x_next = next_frame.get('ball_x', 0)

        ball_dist = np.sqrt(
            (ball_x_next - ball_x_current)**2 +
            (next_frame.get('ball_y', 0) - current_frame.get('ball_y', 0))**2
        )

        # 简化的动作推断：
        # - 球移动距离大 + 向前推进：DRIBBLE (1)
        # - 球移动距离大 + 向侧面或找到新接球者：PASS (2)
        # - 球在禁区附近且进球：SHOOT (0)
        # - 球移动距离小：HOLD (3)

        if next_frame.get('goal_scored', False) or next_frame.get('game_mode') == 'goal_left':
            return 0  # SHOOT
        elif ball_dist > 3.0 and ball_x_next > ball_x_current:
            return 1  # DRIBBLE（向前推进）
        elif ball_dist > 5.0:
            return 2  # PASS（球移动距离大）
        else:
            return 3  # HOLD

    def _calculate_reward(self, current_frame, next_frame):
        """计算奖励"""
        reward = 0.0

        # 进球奖励
        if next_frame.get('goal_scored', False) or next_frame.get('game_mode') == 'goal_left':
            reward += 1000.0

        # 失球惩罚
        if next_frame.get('goal_conceded', False) or next_frame.get('game_mode') == 'goal_right':
            reward -= 1000.0

        # 球推进奖励
        ball_x_current = current_frame.get('ball_x', 0)
        ball_x_next = next_frame.get('ball_x', 0)
        advancement = ball_x_next - ball_x_current
        reward += advancement * 5.0

        # 防守压力惩罚（球靠近己方球门）
        if ball_x_next < -35.0:
            danger_factor = (-35.0 - ball_x_next) / 35.0
            reward -= 20.0 * danger_factor

        return reward


def save_to_csv(data, output_path):
    """保存数据为CSV格式"""
    if not data:
        print("没有数据需要保存")
        return

    # CSV字段
    fieldnames = ['match_num', 'cycle', 'action', 'reward', 'ball_x', 'ball_y',
                  'score_left', 'score_right', 'game_mode']

    # 添加特征列（feature_0 到 feature_349）
    for i in range(350):
        fieldnames.append(f'feature_{i}')

    with open(output_path, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        for sample in data:
            row = {
                'match_num': sample['match_num'],
                'cycle': sample['cycle'],
                'action': sample['action'],
                'reward': sample['reward'],
                'ball_x': sample['ball_x'],
                'ball_y': sample['ball_y'],
                'score_left': sample['score_left'],
                'score_right': sample['score_right'],
                'game_mode': sample['game_mode']
            }

            # 添加特征值
            for i, feat in enumerate(sample['features']):
                row[f'feature_{i}'] = feat

            writer.writerow(row)

    print(f"数据已保存到: {output_path}")
    print(f"总样本数: {len(data)}")


def main():
    parser = argparse.ArgumentParser(description='RoboCup比赛数据提取')
    parser.add_argument('--rcg', required=True, help='RCG日志文件路径')
    parser.add_argument('--output', required=True, help='输出CSV文件路径')
    parser.add_argument('--match-num', type=int, default=0, help='比赛编号')

    args = parser.parse_args()

    # 解析RCG文件
    print(f"解析RCG文件: {args.rcg}")
    rcg_parser = RCGParser(args.rcg)

    if not rcg_parser.parse():
        print("解析失败！")
        return

    print(f"成功解析 {len(rcg_parser.frames)} 帧")
    print(f"比赛信息: {rcg_parser.match_info}")

    # 提取数据
    print("提取训练数据...")
    extractor = MatchDataExtractor(rcg_parser, args.match_num)
    training_data = extractor.extract()

    # 保存CSV
    save_to_csv(training_data, args.output)


if __name__ == '__main__':
    main()
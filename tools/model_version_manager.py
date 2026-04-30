#!/usr/bin/env python3
"""
ML模型权重文件版本管理工具
为权重文件添加版本信息头部，便于追踪模型版本
"""

import sys
import argparse
from pathlib import Path
from datetime import datetime


def add_version_header(weight_file, version, feature_dim, architecture, accuracy, training_samples, notes=""):
    """为权重文件添加版本信息头部"""

    # 读取原始权重数据
    with open(weight_file, 'r') as f:
        original_content = f.read()

    # 检查是否已有版本信息
    if original_content.startswith("# MODEL_VERSION:"):
        print(f"警告: {weight_file} 已包含版本信息")
        response = input("是否覆盖现有版本信息? (y/n): ")
        if response.lower() != 'y':
            print("操作已取消")
            return False

        # 移除旧的版本信息头部
        lines = original_content.split('\n')
        content_start = 0
        for i, line in enumerate(lines):
            if not line.startswith('#'):
                content_start = i
                break
        original_content = '\n'.join(lines[content_start:])

    # 生成版本信息头部
    header = f"""# MODEL_VERSION: {version}
# TRAINED_DATE: {datetime.now().strftime('%Y-%m-%d')}
# FEATURE_DIM: {feature_dim}
# ARCHITECTURE: {architecture}
# ACCURACY: {accuracy}
# TRAINING_SAMPLES: {training_samples}
"""

    if notes:
        header += f"# NOTES: {notes}\n"

    header += "# " + "="*70 + "\n"

    # 写入新文件
    with open(weight_file, 'w') as f:
        f.write(header)
        f.write(original_content)

    print(f"✓ 已为 {weight_file} 添加版本信息")
    return True


def read_version_info(weight_file):
    """读取权重文件的版本信息"""

    if not Path(weight_file).exists():
        print(f"错误: 文件不存在 {weight_file}")
        return None

    version_info = {}

    with open(weight_file, 'r') as f:
        for line in f:
            if not line.startswith('#'):
                break

            if ':' in line:
                key, value = line[1:].split(':', 1)
                key = key.strip()
                value = value.strip()
                version_info[key] = value

    return version_info


def display_version_info(weight_file):
    """显示权重文件的版本信息"""

    info = read_version_info(weight_file)

    if not info:
        print(f"{weight_file}: 无版本信息")
        return

    print(f"\n{'='*70}")
    print(f"模型: {Path(weight_file).name}")
    print(f"{'='*70}")

    for key, value in info.items():
        if key != '=' * 70:
            print(f"{key:20s}: {value}")

    print()


def batch_add_versions(models_dir):
    """批量为模型添加版本信息"""

    models_dir = Path(models_dir)

    # 预定义的模型配置
    model_configs = {
        'shot_target_mlp_weights.txt': {
            'version': '1.0.0',
            'feature_dim': 27,
            'architecture': '27-64-32-16-1',
            'accuracy': '0.8542',
            'training_samples': 9096,
            'notes': 'Initial shot target model with 7-point sampling'
        },
        'unmark_mlp_weights.txt': {
            'version': '1.0.0',
            'feature_dim': 30,
            'architecture': '30-64-32-16-1',
            'accuracy': '0.7823',
            'training_samples': 116437,
            'notes': 'Unmark positioning model with large dataset'
        },
        'pass_decision_mlp_weights.txt': {
            'version': '1.0.0',
            'feature_dim': 35,
            'architecture': '35-64-32-16-1',
            'accuracy': '0.7156',
            'training_samples': 35051,
            'notes': 'Pass decision model with tactical features'
        },
        'defense_positioning_mlp_weights.txt': {
            'version': '0.9.0',
            'feature_dim': 32,
            'architecture': '32-64-32-16-1',
            'accuracy': '0.6500',
            'training_samples': 6400,
            'notes': 'Initial defense model - needs more training data'
        }
    }

    for weight_file, config in model_configs.items():
        file_path = models_dir / weight_file

        if not file_path.exists():
            print(f"跳过: {weight_file} (文件不存在)")
            continue

        add_version_header(
            file_path,
            config['version'],
            config['feature_dim'],
            config['architecture'],
            config['accuracy'],
            config['training_samples'],
            config['notes']
        )


def compare_versions(file1, file2):
    """比较两个权重文件的版本信息"""

    info1 = read_version_info(file1)
    info2 = read_version_info(file2)

    if not info1 or not info2:
        print("无法比较：一个或两个文件缺少版本信息")
        return

    print(f"\n{'='*70}")
    print(f"版本比较")
    print(f"{'='*70}")
    print(f"{'字段':<20s} {'文件1':<25s} {'文件2':<25s}")
    print(f"{'-'*70}")

    all_keys = set(info1.keys()) | set(info2.keys())

    for key in sorted(all_keys):
        if key == '=' * 70:
            continue

        val1 = info1.get(key, 'N/A')
        val2 = info2.get(key, 'N/A')

        marker = '  ' if val1 == val2 else '* '
        print(f"{marker}{key:<18s} {val1:<25s} {val2:<25s}")

    print()


def main():
    parser = argparse.ArgumentParser(
        description='ML模型权重文件版本管理工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  # 为单个权重文件添加版本信息
  %(prog)s add shot_target_mlp_weights.txt --version 1.0.0 --feature-dim 27 \\
      --architecture "27-64-32-16-1" --accuracy 0.8542 --samples 9096

  # 查看权重文件版本信息
  %(prog)s show shot_target_mlp_weights.txt

  # 批量为所有模型添加版本信息
  %(prog)s batch ./build/bin/

  # 比较两个权重文件版本
  %(prog)s compare old_weights.txt new_weights.txt
        """
    )

    subparsers = parser.add_subparsers(dest='command', help='子命令')

    # add命令
    add_parser = subparsers.add_parser('add', help='为权重文件添加版本信息')
    add_parser.add_argument('weight_file', help='权重文件路径')
    add_parser.add_argument('--version', required=True, help='模型版本号 (如 1.0.0)')
    add_parser.add_argument('--feature-dim', type=int, required=True, help='特征维度')
    add_parser.add_argument('--architecture', required=True, help='网络结构 (如 27-64-32-16-1)')
    add_parser.add_argument('--accuracy', required=True, help='验证准确率')
    add_parser.add_argument('--samples', type=int, required=True, help='训练样本数')
    add_parser.add_argument('--notes', default='', help='备注信息')

    # show命令
    show_parser = subparsers.add_parser('show', help='显示权重文件版本信息')
    show_parser.add_argument('weight_file', help='权重文件路径')

    # batch命令
    batch_parser = subparsers.add_parser('batch', help='批量添加版本信息')
    batch_parser.add_argument('models_dir', help='模型目录路径')

    # compare命令
    compare_parser = subparsers.add_parser('compare', help='比较两个权重文件版本')
    compare_parser.add_argument('file1', help='第一个权重文件')
    compare_parser.add_argument('file2', help='第二个权重文件')

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return

    if args.command == 'add':
        add_version_header(
            args.weight_file,
            args.version,
            args.feature_dim,
            args.architecture,
            args.accuracy,
            args.samples,
            args.notes
        )

    elif args.command == 'show':
        display_version_info(args.weight_file)

    elif args.command == 'batch':
        batch_add_versions(args.models_dir)

    elif args.command == 'compare':
        compare_versions(args.file1, args.file2)


if __name__ == '__main__':
    main()

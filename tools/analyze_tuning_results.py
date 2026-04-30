#!/usr/bin/env python3
"""
ML权重调优结果分析脚本
分析tune_ml_weights.sh生成的结果，找出最优权重配置
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path


def load_results(csv_file):
    """加载调优结果"""
    try:
        df = pd.read_csv(csv_file)
        print(f"成功加载 {len(df)} 条结果记录")
        return df
    except Exception as e:
        print(f"加载结果文件失败: {e}")
        sys.exit(1)


def analyze_single_variable(df, var_name, default_values):
    """分析单变量调优结果"""
    print(f"\n{'='*60}")
    print(f"分析 {var_name}")
    print(f"{'='*60}")

    # 筛选出只改变该变量的配置
    mask = True
    for other_var, default_val in default_values.items():
        if other_var != var_name:
            mask = mask & (df[other_var] == default_val)

    subset = df[mask].copy()

    if len(subset) == 0:
        print(f"没有找到 {var_name} 的单变量调优数据")
        return None

    # 按得分排序
    subset = subset.sort_values('score', ascending=False)

    print(f"\n最佳配置:")
    best = subset.iloc[0]
    print(f"  {var_name} = {best[var_name]}")
    print(f"  胜/平/负: {best['wins']}/{best['draws']}/{best['losses']}")
    print(f"  进球/失球: {best['goals_for']}/{best['goals_against']}")
    print(f"  净胜球: {best['goal_diff']}")
    print(f"  综合得分: {best['score']:.2f}")

    print(f"\n所有配置:")
    print(subset[[var_name, 'wins', 'draws', 'losses', 'goal_diff', 'score']].to_string(index=False))

    return subset


def plot_results(df, var_name, output_dir):
    """绘制结果图表"""
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle(f'{var_name} 调优结果', fontsize=16)

    # 得分曲线
    axes[0, 0].plot(df[var_name], df['score'], 'o-', linewidth=2, markersize=8)
    axes[0, 0].set_xlabel(var_name)
    axes[0, 0].set_ylabel('综合得分')
    axes[0, 0].set_title('综合得分 vs ' + var_name)
    axes[0, 0].grid(True, alpha=0.3)

    # 胜率
    total_games = df['wins'] + df['draws'] + df['losses']
    win_rate = df['wins'] / total_games * 100
    axes[0, 1].plot(df[var_name], win_rate, 'o-', color='green', linewidth=2, markersize=8)
    axes[0, 1].set_xlabel(var_name)
    axes[0, 1].set_ylabel('胜率 (%)')
    axes[0, 1].set_title('胜率 vs ' + var_name)
    axes[0, 1].grid(True, alpha=0.3)

    # 净胜球
    axes[1, 0].plot(df[var_name], df['goal_diff'], 'o-', color='blue', linewidth=2, markersize=8)
    axes[1, 0].axhline(y=0, color='red', linestyle='--', alpha=0.5)
    axes[1, 0].set_xlabel(var_name)
    axes[1, 0].set_ylabel('净胜球')
    axes[1, 0].set_title('净胜球 vs ' + var_name)
    axes[1, 0].grid(True, alpha=0.3)

    # 胜/平/负分布
    x = range(len(df))
    axes[1, 1].bar(x, df['wins'], label='胜', color='green', alpha=0.7)
    axes[1, 1].bar(x, df['draws'], bottom=df['wins'], label='平', color='yellow', alpha=0.7)
    axes[1, 1].bar(x, df['losses'], bottom=df['wins']+df['draws'], label='负', color='red', alpha=0.7)
    axes[1, 1].set_xlabel('配置索引')
    axes[1, 1].set_ylabel('比赛场次')
    axes[1, 1].set_title('胜/平/负分布')
    axes[1, 1].legend()
    axes[1, 1].grid(True, alpha=0.3, axis='y')

    plt.tight_layout()

    # 保存图表
    output_file = output_dir / f'{var_name}_analysis.png'
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"图表已保存到: {output_file}")
    plt.close()


def find_best_overall(df):
    """找出总体最佳配置"""
    print(f"\n{'='*60}")
    print("总体最佳配置")
    print(f"{'='*60}")

    best = df.loc[df['score'].idxmax()]

    print(f"\n最佳权重配置:")
    print(f"  LAMBDA_SHOT = {best['LAMBDA_SHOT']}")
    print(f"  LAMBDA_UNMARK = {best['LAMBDA_UNMARK']}")
    print(f"  LAMBDA_PASS = {best['LAMBDA_PASS']}")
    print(f"  LAMBDA_DEFENSE = {best['LAMBDA_DEFENSE']}")
    print(f"\n性能指标:")
    print(f"  胜/平/负: {best['wins']}/{best['draws']}/{best['losses']}")
    print(f"  进球/失球: {best['goals_for']}/{best['goals_against']}")
    print(f"  净胜球: {best['goal_diff']}")
    print(f"  综合得分: {best['score']:.2f}")

    return best


def generate_report(df, output_dir):
    """生成调优报告"""
    report_file = output_dir / 'tuning_report.txt'

    with open(report_file, 'w', encoding='utf-8') as f:
        f.write("ML权重调优报告\n")
        f.write("="*60 + "\n\n")

        # 总体统计
        f.write(f"总配置数: {len(df)}\n")
        f.write(f"总比赛场次: {df['wins'].sum() + df['draws'].sum() + df['losses'].sum()}\n")
        f.write(f"总进球数: {df['goals_for'].sum()}\n")
        f.write(f"总失球数: {df['goals_against'].sum()}\n\n")

        # 最佳配置
        best = df.loc[df['score'].idxmax()]
        f.write("最佳配置:\n")
        f.write(f"  LAMBDA_SHOT = {best['LAMBDA_SHOT']}\n")
        f.write(f"  LAMBDA_UNMARK = {best['LAMBDA_UNMARK']}\n")
        f.write(f"  LAMBDA_PASS = {best['LAMBDA_PASS']}\n")
        f.write(f"  LAMBDA_DEFENSE = {best['LAMBDA_DEFENSE']}\n")
        f.write(f"  综合得分: {best['score']:.2f}\n\n")

        # Top 5配置
        f.write("Top 5 配置:\n")
        top5 = df.nlargest(5, 'score')
        f.write(top5.to_string(index=False))
        f.write("\n\n")

        # 建议
        f.write("优化建议:\n")
        f.write("1. 将最佳权重配置应用到代码中\n")
        f.write("2. 在更多对手上验证性能\n")
        f.write("3. 考虑进行多变量联合优化\n")

    print(f"\n报告已保存到: {report_file}")


def main():
    if len(sys.argv) < 2:
        print("用法: python3 analyze_tuning_results.py <results.csv>")
        sys.exit(1)

    csv_file = Path(sys.argv[1])
    if not csv_file.exists():
        print(f"文件不存在: {csv_file}")
        sys.exit(1)

    # 加载结果
    df = load_results(csv_file)

    # 输出目录
    output_dir = csv_file.parent / 'analysis'
    output_dir.mkdir(exist_ok=True)

    # 默认权重值
    default_values = {
        'LAMBDA_SHOT': 50000,
        'LAMBDA_UNMARK': 10.0,
        'LAMBDA_PASS': 50.0,
        'LAMBDA_DEFENSE': 30.0
    }

    # 分析每个变量
    for var_name in ['LAMBDA_SHOT', 'LAMBDA_UNMARK', 'LAMBDA_PASS', 'LAMBDA_DEFENSE']:
        subset = analyze_single_variable(df, var_name, default_values)
        if subset is not None and len(subset) > 1:
            plot_results(subset, var_name, output_dir)

    # 找出总体最佳配置
    find_best_overall(df)

    # 生成报告
    generate_report(df, output_dir)

    print(f"\n分析完成！结果保存在: {output_dir}")


if __name__ == '__main__':
    main()

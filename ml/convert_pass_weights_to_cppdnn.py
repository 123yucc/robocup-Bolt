#!/usr/bin/env python3
"""
将PyTorch传球MLP权重转换为CppDNN格式
"""
import torch
import sys

def convert_to_cppdnn(pytorch_weights_path, output_path):
    """转换PyTorch权重到CppDNN格式"""

    # 加载PyTorch权重
    state_dict = torch.load(pytorch_weights_path, map_location='cpu')

    # 网络结构: 35 -> 64 -> 32 -> 16 -> 1
    layers = [
        ('net.0', 64, 35, 'relu'),
        ('net.2', 32, 64, 'relu'),
        ('net.4', 16, 32, 'relu'),
        ('net.6', 1, 16, 'sigmoid')
    ]

    with open(output_path, 'w') as f:
        # 写入层数
        f.write(f"# Layer Numbers: {len(layers)}\n")

        for layer_idx, (layer_name, out_dim, in_dim, activation) in enumerate(layers):
            f.write(f"# Layer Number: {layer_idx}\n")
            f.write(f"{activation}\n")
            f.write(f"{out_dim} {in_dim}\n")

            # 写入权重矩阵 (转置: PyTorch是out×in, CppDNN需要逐行写入)
            weight_key = f"{layer_name}.weight"
            bias_key = f"{layer_name}.bias"

            if weight_key not in state_dict:
                print(f"警告: 找不到权重 {weight_key}")
                continue

            weight = state_dict[weight_key].numpy()  # shape: (out_dim, in_dim)
            bias = state_dict[bias_key].numpy()      # shape: (out_dim,)

            # 写入权重 (按行展开)
            f.write("# W\n")
            for row in weight:
                for val in row:
                    f.write(f"{val:.8f}\n")

            # 写入偏置
            f.write("# b\n")
            for val in bias:
                f.write(f"{val:.8f}\n")

    total_params = sum((out*in_dim + out) for _, out, in_dim, _ in layers)
    print(f"✓ 转换完成: {output_path}")
    print(f"  - 层数: {len(layers)}")
    print(f"  - 参数总数: {total_params}")

if __name__ == '__main__':
    pytorch_weights = '/home/yucc/robocup-Bolt/ml/models/pass_decision_mlp_best.pth'
    output_path = '/home/yucc/robocup-Bolt/build_v4/bin/pass_decision_mlp_weights.txt'

    convert_to_cppdnn(pytorch_weights, output_path)

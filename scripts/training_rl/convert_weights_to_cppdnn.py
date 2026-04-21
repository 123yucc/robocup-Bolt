#!/usr/bin/env python3
"""
Bolt Team - 权重转换脚本

将训练好的 Keras/TensorFlow .h5 模型转换为 CppDNN .txt 格式
供 C++ 程序加载使用

CppDNN 格式（参考 unmark_dnn_weights.txt）：
# Layer Numbers: N
# Layer Number: 0
relu
128 290
# W
<weight values一行>
# B
<bias values一行>
...

使用方法：
    python3 convert_weights_to_cppdnn.py --model weights/value_network.h5 --output weights/rl_value.txt
"""

import argparse
import os
import numpy as np

try:
    import tensorflow as tf
    from tensorflow import keras
    TF_AVAILABLE = True
except ImportError:
    TF_AVAILABLE = False
    print("警告: TensorFlow未安装，无法加载.h5模型")


def convert_keras_to_cppdnn(model_path, output_path, model_type='value'):
    """
    将Keras模型转换为CppDNN格式

    Args:
        model_path: Keras .h5模型文件路径
        output_path: CppDNN .txt输出路径
        model_type: 'value' 或 'policy'（影响输出层处理）
    """
    if not TF_AVAILABLE:
        print("错误: 需要TensorFlow才能加载.h5模型")
        return False

    print(f"加载模型: {model_path}")

    try:
        model = keras.models.load_model(model_path, compile=False)
        print(f"模型加载成功")
        print(f"模型架构:")
        model.summary()
    except Exception as e:
        print(f"错误: 无法加载模型 - {e}")
        return False

    # 提取层信息
    layers_with_weights = []
    for layer in model.layers:
        weights = layer.get_weights()
        if len(weights) > 0:
            layers_with_weights.append(layer)

    print(f"找到 {len(layers_with_weights)} 个有权重的层")

    # 写入CppDNN格式
    with open(output_path, 'w') as f:
        # 文件头
        f.write(f"# Bolt Team - RL {model_type} Network Weights\n")
        f.write(f"# Converted from: {model_path}\n")
        f.write(f"# Format: CppDNN\n")
        f.write(f"# Layer Numbers: {len(layers_with_weights)}\n")
        f.write("\n")

        for i, layer in enumerate(layers_with_weights):
            weights = layer.get_weights()

            if len(weights) == 2:  # Dense层：W和B
                W, B = weights

                # 获取激活函数名称
                activation = 'linear'
                if hasattr(layer, 'activation'):
                    activation_name = layer.activation.__name__
                    # CppDNN支持的激活函数名称映射
                    activation_map = {
                        'relu': 'relu',
                        'sigmoid': 'sigmoid',
                        'tanh': 'tanh',
                        'linear': 'linear',
                        'softmax': 'softmax'
                    }
                    activation = activation_map.get(activation_name, 'relu')

                f.write(f"# Layer Number: {i}\n")
                f.write(f"{activation}\n")

                # 维度：CppDNN格式是 output_dim, input_dim
                # 注意：需要转置W矩阵
                output_dim = W.shape[1]
                input_dim = W.shape[0]
                f.write(f"{output_dim} {input_dim}\n")

                # 权重矩阵（转置后一行一行写入）
                f.write("# W\n")
                W_T = W.T  # 转置
                # CppDNN期望权重矩阵按列存储（每行是一个输出单元的所有输入权重）
                # 所以我们需要按行写入转置后的矩阵
                for row in W_T:
                    f.write(' '.join([f'{v:.8f}' for v in row]) + '\n')

                # 偏置向量
                f.write("# B\n")
                f.write(' '.join([f'{v:.8f}' for v in B]) + '\n')

                f.write("\n")

            elif len(weights) == 1:  # 只有偏置的层
                B = weights[0]
                f.write(f"# Layer Number: {i}\n")
                f.write("linear\n")
                f.write(f"{len(B)} 0\n")  # 输入维度为0（只有偏置）
                f.write("# W\n")  # 空权重
                f.write("# B\n")
                f.write(' '.join([f'{v:.8f}' for v in B]) + '\n')
                f.write("\n")

    print(f"CppDNN权重已保存到: {output_path}")
    return True


def convert_numpy_to_cppdnn(weights_dict, output_path):
    """
    将numpy权重字典转换为CppDNN格式
    （用于没有TensorFlow的情况）
    """
    print(f"转换numpy权重到: {output_path}")

    # 简单的2层网络格式
    with open(output_path, 'w') as f:
        f.write("# Bolt Team - RL Value Network Weights (numpy)\n")
        f.write("# Format: CppDNN\n")
        f.write("# Layer Numbers: 2\n")
        f.write("\n")

        # Layer 0: hidden layer
        f.write("# Layer Number: 0\n")
        f.write("relu\n")
        W1 = weights_dict['W1']
        f.write(f"{W1.shape[1]} {W1.shape[0]}\n")
        f.write("# W\n")
        for row in W1.T:
            f.write(' '.join([f'{v:.8f}' for v in row]) + '\n')
        f.write("# B\n")
        f.write(' '.join([f'{v:.8f}' for v in weights_dict['b1']]) + '\n')
        f.write("\n")

        # Layer 1: output layer
        f.write("# Layer Number: 1\n")
        f.write("linear\n")
        W2 = weights_dict['W2']
        f.write(f"{W2.shape[1]} {W2.shape[0]}\n")
        f.write("# W\n")
        for row in W2.T:
            f.write(' '.join([f'{v:.8f}' for v in row]) + '\n')
        f.write("# B\n")
        f.write(' '.join([f'{v:.8f}' for v in weights_dict['b2']]) + '\n')

    print(f"已保存: {output_path}")


def verify_cppdnn_format(filepath):
    """
    验证CppDNN格式文件是否正确
    """
    print(f"验证文件: {filepath}")

    if not os.path.exists(filepath):
        print(f"错误: 文件不存在")
        return False

    with open(filepath, 'r') as f:
        content = f.read()

    # 检查基本格式
    if '# Layer Numbers:' not in content:
        print("错误: 缺少Layer Numbers声明")
        return False

    if '# Layer Number:' not in content:
        print("错误: 缺少Layer Number声明")
        return False

    if '# W' not in content or '# B' not in content:
        print("错误: 缺少权重或偏置声明")
        return False

    print("格式验证通过 ✓")
    return True


def compare_with_reference(cppdnn_path, reference_path):
    """
    与参考格式文件对比（如 unmark_dnn_weights.txt）
    """
    if not os.path.exists(reference_path):
        print(f"参考文件不存在: {reference_path}")
        return

    print(f"\n对比参考文件: {reference_path}")

    with open(reference_path, 'r') as f:
        ref_content = f.read()

    with open(cppdnn_path, 'r') as f:
        new_content = f.read()

    # 提取结构信息
    ref_layers = ref_content.count('# Layer Number:')
    new_layers = new_content.count('# Layer Number:')

    print(f"参考文件层数: {ref_layers}")
    print(f"新文件层数: {new_layers}")

    if ref_layers == new_layers:
        print("层数匹配 ✓")
    else:
        print("警告: 层数不匹配")


def main():
    parser = argparse.ArgumentParser(description='转换权重到CppDNN格式')

    parser.add_argument('--model', required=True, help='Keras .h5 模型文件路径')
    parser.add_argument('--output', required=True, help='CppDNN .txt 输出路径')
    parser.add_argument('--type', default='value', choices=['value', 'policy'],
                       help='模型类型 (value/policy)')
    parser.add_argument('--reference', default='',
                       help='参考格式文件路径（用于对比验证）')
    parser.add_argument('--numpy', default='',
                       help='numpy权重文件路径（备用，当无TF时）')

    args = parser.parse_args()

    print("="*60)
    print("Bolt Team - 权重转换工具")
    print("="*60)

    # 转换
    if TF_AVAILABLE and args.model.endswith('.h5'):
        success = convert_keras_to_cppdnn(args.model, args.output, args.type)
    elif args.numpy:
        weights = np.load(args.numpy, allow_pickle=True).item()
        convert_numpy_to_cppdnn(weights, args.output)
        success = True
    else:
        print("错误: 需要TensorFlow加载.h5模型，或提供numpy权重文件")
        success = False

    if success:
        # 验证
        verify_cppdnn_format(args.output)

        # 对比参考
        if args.reference:
            compare_with_reference(args.output, args.reference)

        print("\n" + "="*60)
        print("转换完成!")
        print("="*60)
        print(f"\nCppDNN权重文件: {args.output}")
        print(f"\n下一步: 在 C++ 中加载权重")
        print(f"  evaluator->loadWeights(\"{args.output}\", \"\")")


if __name__ == '__main__':
    main()
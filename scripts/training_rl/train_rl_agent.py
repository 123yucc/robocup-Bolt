#!/usr/bin/env python3
"""
Bolt Team - RoboCup 2D 2026
Reinforcement Learning Training Script

Implements DQN + Policy Gradient (Actor-Critic) for soccer agent training
Based on Cyrus2DBase existing ML infrastructure

Usage:
    python train_rl_agent.py --data path/to/experiences.csv --epochs 1000
"""

import os
import sys
import argparse
import numpy as np
import json
from collections import deque
import random

# Try to import TensorFlow/Keras
try:
    import tensorflow as tf
    from tensorflow import keras
    from tensorflow.keras import layers, models, optimizers, regularizers
    TF_AVAILABLE = True
    print("TensorFlow version:", tf.__version__)
except ImportError:
    TF_AVAILABLE = False
    print("WARNING: TensorFlow not available. Using numpy-based fallback implementation.")


class CyrusRLTrainer:
    """Reinforcement Learning trainer for Cyrus2D soccer agent"""

    def __init__(self, state_dim=350, action_dim=4, learning_rate=0.0001, gamma=0.99):
        """
        Initialize the RL trainer

        Args:
            state_dim: State feature dimension (350 for RL features)
            action_dim: Number of actions (4: Pass, Shoot, Dribble, Hold)
            learning_rate: Learning rate for optimizer
            gamma: Discount factor for future rewards
        """
        self.state_dim = state_dim
        self.action_dim = action_dim
        self.learning_rate = learning_rate
        self.gamma = gamma

        # Replay buffer
        self.replay_buffer = deque(maxlen=50000)

        # Training statistics
        self.training_stats = {
            'epochs': [],
            'value_loss': [],
            'policy_loss': [],
            'avg_reward': [],
            'episode_rewards': []
        }

        # Build networks
        if TF_AVAILABLE:
            self._build_keras_networks()
        else:
            self._build_numpy_networks()

    def _build_keras_networks(self):
        """Build neural networks using Keras/TensorFlow"""

        # Shared feature extractor layers
        shared_layers = [
            layers.Dense(512, activation='relu',
                        kernel_regularizer=regularizers.l2(0.001),
                        input_shape=(self.state_dim,)),
            layers.LayerNormalization(),
            layers.Dropout(0.2),
            layers.Dense(256, activation='relu'),
            layers.LayerNormalization(),
            layers.Dropout(0.2),
            layers.Dense(128, activation='relu'),
        ]

        # Value network (DQN critic) - estimates V(s)
        value_input = layers.Input(shape=(self.state_dim,))
        shared = value_input
        for layer in shared_layers:
            shared = layer(shared)
        value_output = layers.Dense(1, activation='linear', name='value')(shared)
        self.value_network = models.Model(value_input, value_output, name='value_network')
        self.value_network.compile(
            optimizer=optimizers.Adam(learning_rate=self.learning_rate),
            loss='mse'
        )

        # Policy network (Actor) - estimates action probabilities
        policy_input = layers.Input(shape=(self.state_dim,))
        shared = policy_input
        for layer in shared_layers:
            shared = layer(shared)
        policy_output = layers.Dense(self.action_dim, activation='softmax', name='policy')(shared)
        self.policy_network = models.Model(policy_input, policy_output, name='policy_network')
        self.policy_network.compile(
            optimizer=optimizers.Adam(learning_rate=self.learning_rate),
            loss='categorical_crossentropy'
        )

        print("Built Keras networks:")
        print(f"  Value network: {self.value_network.summary()}")
        print(f"  Policy network: {self.policy_network.summary()}")

    def _build_numpy_networks(self):
        """Build simple numpy-based networks for fallback"""

        # Simple weight initialization
        self.value_weights = {
            'W1': np.random.randn(self.state_dim, 128) * 0.1,
            'b1': np.zeros(128),
            'W2': np.random.randn(128, 1) * 0.1,
            'b2': np.zeros(1)
        }

        self.policy_weights = {
            'W1': np.random.randn(self.state_dim, 128) * 0.1,
            'b1': np.zeros(128),
            'W2': np.random.randn(128, self.action_dim) * 0.1,
            'b2': np.zeros(self.action_dim)
        }

        print("Built numpy fallback networks")

    def load_experience_csv(self, filepath):
        """
        Load experience data from CSV file

        CSV format: state_features;action;reward;next_state_features;is_terminal;cycle;player_unum
        """
        experiences = []

        if not os.path.exists(filepath):
            print(f"Warning: File not found: {filepath}")
            return experiences

        with open(filepath, 'r') as f:
            for line in f:
                # Skip comments and empty lines
                if line.startswith('#') or line.strip() == '':
                    continue

                parts = line.strip().split(';')
                if len(parts) < 5:
                    continue

                try:
                    # Parse state features
                    state = np.array([float(x) for x in parts[0].split(',')])

                    # Parse action
                    action = int(parts[1])

                    # Parse reward
                    reward = float(parts[2])

                    # Parse next state features
                    next_state = np.array([float(x) for x in parts[3].split(',')])

                    # Parse terminal flag
                    terminal = bool(int(parts[4]))

                    # Parse cycle and player (optional)
                    cycle = int(parts[5]) if len(parts) > 5 else 0
                    player_unum = int(parts[6]) if len(parts) > 6 else 0

                    experiences.append({
                        'state': state,
                        'action': action,
                        'reward': reward,
                        'next_state': next_state,
                        'terminal': terminal,
                        'cycle': cycle,
                        'player_unum': player_unum
                    })
                except Exception as e:
                    print(f"Error parsing line: {e}")
                    continue

        print(f"Loaded {len(experiences)} experiences from {filepath}")
        return experiences

    def add_to_buffer(self, experiences):
        """Add experiences to replay buffer"""
        for exp in experiences:
            self.replay_buffer.append(exp)

    def sample_batch(self, batch_size=64):
        """Sample random batch from replay buffer"""
        if len(self.replay_buffer) < batch_size:
            return None

        batch = random.sample(self.replay_buffer, batch_size)
        return batch

    def compute_td_target(self, reward, next_state, terminal):
        """Compute TD target for DQN"""
        if terminal:
            return reward

        # Estimate next state value
        if TF_AVAILABLE:
            next_value = self.value_network.predict(next_state.reshape(1, -1), verbose=0)[0][0]
        else:
            next_value = self._numpy_predict_value(next_state)

        return reward + self.gamma * next_value

    def _numpy_predict_value(self, state):
        """Simple numpy-based value prediction"""
        h = np.maximum(0, np.dot(state, self.value_weights['W1']) + self.value_weights['b1'])  # ReLU
        return np.dot(h, self.value_weights['W2']) + self.value_weights['b2']

    def train_dqn_epoch(self, batch_size=64):
        """Train one epoch using DQN"""
        batch = self.sample_batch(batch_size)
        if batch is None:
            return None, None

        # Prepare batch data
        states = np.array([exp['state'] for exp in batch])
        actions = np.array([exp['action'] for exp in batch])
        rewards = np.array([exp['reward'] for exp in batch])
        next_states = np.array([exp['next_state'] for exp in batch])
        terminals = np.array([exp['terminal'] for exp in batch])

        # Compute TD targets
        td_targets = np.array([
            self.compute_td_target(r, ns, t)
            for r, ns, t in zip(rewards, next_states, terminals)
        ])

        if TF_AVAILABLE:
            # Train value network
            value_loss = self.value_network.train_on_batch(states, td_targets)

            # Compute advantages for policy gradient
            current_values = self.value_network.predict(states, verbose=0).flatten()
            advantages = td_targets - current_values

            # Normalize advantages
            advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)

            # Create action labels for policy gradient
            action_labels = np.zeros((batch_size, self.action_dim))
            for i, action in enumerate(actions):
                action_labels[i, action] = 1.0

            # Train policy network with advantage weights
            policy_loss = self.policy_network.train_on_batch(
                states, action_labels,
                sample_weight=advantages
            )
        else:
            # Fallback numpy training (simple gradient descent)
            value_loss = self._numpy_train_value(states, td_targets)
            policy_loss = 0.0

        return value_loss, policy_loss

    def _numpy_train_value(self, states, targets):
        """Simple numpy-based value network training"""
        lr = self.learning_rate

        # Forward pass
        h = np.maximum(0, np.dot(states, self.value_weights['W1']) + self.value_weights['b1'])
        predictions = np.dot(h, self.value_weights['W2']) + self.value_weights['b2']

        # Compute loss
        loss = np.mean((predictions.flatten() - targets) ** 2)

        # Backward pass (simple gradient descent)
        grad_output = 2 * (predictions.flatten() - targets) / len(targets)
        grad_W2 = np.dot(h.T, grad_output.reshape(-1, 1))
        grad_b2 = grad_output.sum()

        grad_h = np.dot(grad_output.reshape(-1, 1), self.value_weights['W2'].T)
        grad_h[h <= 0] = 0  # ReLU gradient

        grad_W1 = np.dot(states.T, grad_h)
        grad_b1 = grad_h.sum(axis=0)

        # Update weights
        self.value_weights['W1'] -= lr * grad_W1
        self.value_weights['b1'] -= lr * grad_b1
        self.value_weights['W2'] -= lr * grad_W2
        self.value_weights['b2'] -= lr * grad_b2

        return loss

    def train(self, epochs=1000, batch_size=64, log_interval=100):
        """
        Train the RL agent

        Args:
            epochs: Number of training epochs
            batch_size: Batch size for each update
            log_interval: Log statistics every N epochs
        """
        print(f"Starting training for {epochs} epochs...")

        for epoch in range(epochs):
            value_loss, policy_loss = self.train_dqn_epoch(batch_size)

            if value_loss is None:
                print(f"Epoch {epoch}: Not enough samples in buffer")
                continue

            # Compute average reward for this epoch
            if len(self.replay_buffer) > 0:
                avg_reward = np.mean([exp['reward'] for exp in list(self.replay_buffer)[-1000:]])
            else:
                avg_reward = 0.0

            # Store statistics
            self.training_stats['epochs'].append(epoch)
            self.training_stats['value_loss'].append(value_loss)
            self.training_stats['policy_loss'].append(policy_loss)
            self.training_stats['avg_reward'].append(avg_reward)

            if epoch % log_interval == 0:
                print(f"Epoch {epoch}: value_loss={value_loss:.4f}, policy_loss={policy_loss:.4f}, avg_reward={avg_reward:.2f}")

        print("Training completed!")

    def save_weights(self, output_dir):
        """Save network weights to files"""
        os.makedirs(output_dir, exist_ok=True)

        if TF_AVAILABLE:
            # Save Keras models
            self.value_network.save(os.path.join(output_dir, 'value_network.h5'))
            self.policy_network.save(os.path.join(output_dir, 'policy_network.h5'))
            print(f"Saved Keras models to {output_dir}")
        else:
            # Save numpy weights
            np.save(os.path.join(output_dir, 'value_weights.npy'), self.value_weights)
            np.save(os.path.join(output_dir, 'policy_weights.npy'), self.policy_weights)
            print(f"Saved numpy weights to {output_dir}")

        # Save training statistics
        with open(os.path.join(output_dir, 'training_stats.json'), 'w') as f:
            json.dump(self.training_stats, f)

    def convert_to_cppdnn(self, output_path):
        """
        Convert weights to CppDNN format for C++ inference

        CppDNN format (from unmark_dnn_weights.txt):
        # Layer Numbers: N
        # Layer Number: 0
        relu
        128 290
        # W
        <weight values>
        <bias values>
        ...
        """
        print(f"Converting weights to CppDNN format: {output_path}")

        with open(output_path, 'w') as f:
            if TF_AVAILABLE:
                # Convert Keras weights
                layers = []
                for layer in self.value_network.layers:
                    if hasattr(layer, 'get_weights') and len(layer.get_weights()) > 0:
                        layers.append(layer)

                f.write(f"# Layer Numbers: {len(layers)}\n")

                for i, layer in enumerate(layers):
                    weights = layer.get_weights()
                    if len(weights) == 2:
                        W, B = weights
                        activation = layer.activation.__name__ if hasattr(layer, 'activation') else 'linear'

                        f.write(f"# Layer Number: {i}\n")
                        f.write(f"{activation}\n")
                        f.write(f"{W.shape[1]} {W.shape[0]}\n")  # Output dim, input dim
                        f.write("# W\n")

                        # Write weight matrix (transposed for CppDNN)
                        for row in W.T:
                            f.write(' '.join(map(str, row)) + '\n')

                        f.write("# B\n")
                        f.write(' '.join(map(str, B)) + '\n')
            else:
                # Convert numpy weights
                f.write("# Layer Numbers: 2\n")

                # Layer 0
                f.write("# Layer Number: 0\n")
                f.write("relu\n")
                f.write(f"{self.value_weights['W1'].shape[1]} {self.value_weights['W1'].shape[0]}\n")
                f.write("# W\n")
                for row in self.value_weights['W1'].T:
                    f.write(' '.join(map(str, row)) + '\n')
                f.write("# B\n")
                f.write(' '.join(map(str, self.value_weights['b1'])) + '\n')

                # Layer 1
                f.write("# Layer Number: 1\n")
                f.write("linear\n")
                f.write(f"{self.value_weights['W2'].shape[1]} {self.value_weights['W2'].shape[0]}\n")
                f.write("# W\n")
                for row in self.value_weights['W2'].T:
                    f.write(' '.join(map(str, row)) + '\n')
                f.write("# B\n")
                f.write(' '.join(map(str, self.value_weights['b2'])) + '\n')

        print(f"Saved CppDNN weights to {output_path}")


def main():
    parser = argparse.ArgumentParser(description='Train RL agent for RoboCup 2D')
    parser.add_argument('--data', type=str, default='',
                       help='Path to experience CSV file')
    parser.add_argument('--epochs', type=int, default=1000,
                       help='Number of training epochs')
    parser.add_argument('--batch_size', type=int, default=64,
                       help='Batch size for training')
    parser.add_argument('--lr', type=float, default=0.0001,
                       help='Learning rate')
    parser.add_argument('--gamma', type=float, default=0.99,
                       help='Discount factor')
    parser.add_argument('--output', type=str, default='./rl_weights',
                       help='Output directory for weights')
    parser.add_argument('--cppdnn', type=str, default='./rl_value_weights.txt',
                       help='Output path for CppDNN format weights')

    args = parser.parse_args()

    # Create trainer
    trainer = CyrusRLTrainer(
        state_dim=350,
        action_dim=4,
        learning_rate=args.lr,
        gamma=args.gamma
    )

    # Load experience data if provided
    if args.data:
        experiences = trainer.load_experience_csv(args.data)
        trainer.add_to_buffer(experiences)

    # If no data provided, generate dummy data for testing
    if len(trainer.replay_buffer) == 0:
        print("Generating dummy experiences for testing...")
        for _ in range(1000):
            state = np.random.randn(350)
            action = np.random.randint(0, 4)
            reward = np.random.randn() * 10
            next_state = np.random.randn(350)
            terminal = np.random.random() < 0.01

            trainer.replay_buffer.append({
                'state': state,
                'action': action,
                'reward': reward,
                'next_state': next_state,
                'terminal': terminal
            })

    # Train
    trainer.train(epochs=args.epochs, batch_size=args.batch_size)

    # Save weights
    trainer.save_weights(args.output)

    # Convert to CppDNN format
    trainer.convert_to_cppdnn(args.cppdnn)

    print("Done!")


if __name__ == '__main__':
    main()
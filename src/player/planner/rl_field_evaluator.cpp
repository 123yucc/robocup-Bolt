// -*-c++-*-
/*
 * Bolt Team - RoboCup 2D 2026
 * Reinforcement Learning Field Evaluator Implementation
 *
 * Strategy: Use original SampleFieldEvaluator as base, add RL enhancement when available
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rl_field_evaluator.h"
#include "sample_field_evaluator.h"  // Include original evaluator
#include "field_analyzer.h"
#include "data_extractor/offensive_data_extractor.h"
#include "data_extractor/DEState.h"

#include <rcsc/common/server_param.h>
#include <rcsc/common/logger.h>
#include <rcsc/player/intercept_table.h>

#include <algorithm>
#include <cmath>
#include <fstream>

using namespace rcsc;

/*-------------------------------------------------------------------*/
RLFieldEvaluator::RLFieldEvaluator(int state_dim)
    : M_state_dim(state_dim)
    , M_base_evaluator(new SampleFieldEvaluator())
    , M_learning_enabled(false)
    , M_alpha(0.9)
    , M_weights_loaded(false)
    , M_total_predictions(0)
    , M_total_value(0.0)
    , M_prev_action(0)
    , M_value_weights_path("")
    , M_policy_weights_path("")
{
    // Create experience buffer
    M_experience_buffer = std::make_shared<RLExperienceBuffer>(50000, state_dim);

    dlog.addText(Logger::TEAM,
                 __FILE__": (RLFieldEvaluator) Created. Base=SampleFieldEvaluator, alpha=%.2f",
                 M_alpha);
}

/*-------------------------------------------------------------------*/
RLFieldEvaluator::~RLFieldEvaluator()
{
    dlog.addText(Logger::TEAM,
                 __FILE__": (RLFieldEvaluator) Destroyed. predictions=%d, avg=%.2f",
                 M_total_predictions, getAverageValue());
}

/*-------------------------------------------------------------------*/
double
RLFieldEvaluator::operator()(const PredictState & state,
                             const std::vector<ActionStatePair> & path,
                             const WorldModel & wm) const
{
    // 1. Always use original SampleFieldEvaluator as base
    double base_value = (*M_base_evaluator)(state, path, wm);

    // 2. Collect training data periodically
    if (wm.time().cycle() % 5 == 0)  // Every 5 cycles
    {
        try
        {
            DEState de_state(wm);
            std::vector<double> features = OffensiveDataExtractor::i().get_data(de_state);

            if (!features.empty())
            {
                // Compute reward
                double reward = M_reward_calculator.calculateReward(wm, wm);

                // Store experience for training
                RLExperienceBuffer::Experience exp;
                exp.state_features = features;
                exp.reward = reward;
                exp.cycle = wm.time().cycle();
                exp.player_unum = wm.self().unum();
                exp.is_terminal = false;

                // Determine action from path
                exp.action_index = HOLD;  // Default
                if (!path.empty())
                {
                    // Map action category to index
                    const CooperativeAction & last_action = path.back().action();
                    CooperativeAction::ActionCategory cat = last_action.category();
                    if (cat == CooperativeAction::Shoot) exp.action_index = SHOOT;
                    else if (cat == CooperativeAction::Pass) exp.action_index = PASS;
                    else if (cat == CooperativeAction::Dribble) exp.action_index = DRIBBLE;
                    else if (cat == CooperativeAction::Hold) exp.action_index = HOLD;
                    else exp.action_index = DRIBBLE;  // Default for other actions
                }

                M_experience_buffer->addExperience(exp);

                // Save to CSV periodically
                if (wm.time().cycle() % 1000 == 0)
                {
                    std::string csv_path = "/home/linna/Cyrus2DBase/logs/experiences.csv";
                    M_experience_buffer->saveToCSV(csv_path);
                    dlog.addText(Logger::TEAM,
                                 __FILE__": Saved %zu experiences to %s",
                                 M_experience_buffer->size(), csv_path.c_str());
                }
            }
        }
        catch (const std::exception & e)
        {
            dlog.addText(Logger::TEAM, __FILE__": Data collection error: %s", e.what());
        }
    }

    // 3. RL enhancement only if weights loaded
    double rl_enhancement = 0.0;

    if (M_weights_loaded)
    {
        try
        {
            DEState de_state(wm);
            std::vector<double> features = OffensiveDataExtractor::i().get_data(de_state);

            if (!features.empty())
            {
                double rl_value = predictValue(features);
                rl_enhancement = rl_value * (1.0 - M_alpha);
            }
        }
        catch (...)
        {
            rl_enhancement = 0.0;
        }
    }

    // 4. Combine: base + RL enhancement
    double combined_value = base_value + rl_enhancement;

    M_total_predictions++;
    M_total_value += combined_value;

    return combined_value;
}

/*-------------------------------------------------------------------*/
double
RLFieldEvaluator::predictValue(const std::vector<double> & features) const
{
    if (features.empty())
    {
        return 10.0;  // Default neutral value
    }

    double value = 0.0;

    try
    {
        // Simple heuristic-based value estimation (placeholder for DNN)
        // Keep values in reasonable range matching heuristic evaluator

        // Extract ball position from features (first few elements)
        double ball_x = 0.0;
        double ball_y = 0.0;

        if (features.size() >= 4)
        {
            // Features format: normalized ball_x, ball_y, ball_r, ball_theta
            // OffensiveDataExtractor normalizes: (x + 52.5) / 105.0
            ball_x = features[0] * 105.0 - 52.5;  // Convert back to field coordinates
            ball_y = features[1] * 68.0 - 34.0;
        }

        // Value based on ball position (higher when closer to opponent goal)
        // Scale to match heuristic range: 0-50
        value = (ball_x + 52.5) * 0.1 + 10.0;  // Range 10 to ~15

        // Penalty for ball near our goal (defensive danger)
        if (ball_x < -35.0)
        {
            value -= 5.0;  // Slight penalty, not huge
        }

        // Bonus for ball near opponent goal (attacking advantage)
        if (ball_x > 35.0)
        {
            value += 10.0;  // Bonus for attacking position
        }

        // SAFETY: Clamp to reasonable range matching operator()
        value = std::max(0.0, std::min(100.0, value));

    }
    catch (const std::exception & e)
    {
        dlog.addText(Logger::TEAM,
                     __FILE__": (predictValue) Exception: %s",
                     e.what());
        return 10.0;  // Safe default
    }

    return value;
}

/*-------------------------------------------------------------------*/
std::vector<double>
RLFieldEvaluator::predictPolicy(const std::vector<double> & features) const
{
    std::vector<double> policy(NUM_ACTIONS, 0.25);  // Default uniform distribution

    if (features.empty())
    {
        return policy;
    }

    // Use policy network to get action probabilities
    // Placeholder implementation - use heuristic-based action selection

    // Extract ball position
    double ball_x = 0.0;
    if (features.size() >= 4)
    {
        ball_x = features[0] * 52.5;
    }

    // Simple heuristic action selection
    if (ball_x > 40.0)  // Near opponent goal
    {
        policy[SHOOT] = 0.6;  // Favor shooting
        policy[DRIBBLE] = 0.2;
        policy[PASS] = 0.15;
        policy[HOLD] = 0.05;
    }
    else if (ball_x > 20.0)  // Mid-attack zone
    {
        policy[DRIBBLE] = 0.4;
        policy[PASS] = 0.35;
        policy[SHOOT] = 0.15;
        policy[HOLD] = 0.1;
    }
    else if (ball_x > -20.0)  // Neutral zone
    {
        policy[PASS] = 0.5;
        policy[DRIBBLE] = 0.25;
        policy[HOLD] = 0.2;
        policy[SHOOT] = 0.05;
    }
    else  // Defensive zone
    {
        policy[PASS] = 0.4;  // Clear the ball
        policy[HOLD] = 0.3;
        policy[DRIBBLE] = 0.2;
        policy[SHOOT] = 0.1;
    }

    return policy;
}

/*-------------------------------------------------------------------*/
int
RLFieldEvaluator::getBestAction(const std::vector<double> & features) const
{
    std::vector<double> policy = predictPolicy(features);

    // Find action with highest probability
    int best_action = 0;
    double max_prob = policy[0];

    for (int i = 1; i < NUM_ACTIONS; ++i)
    {
        if (policy[i] > max_prob)
        {
            max_prob = policy[i];
            best_action = i;
        }
    }

    return best_action;
}

/*-------------------------------------------------------------------*/
double
RLFieldEvaluator::updateFromExperience(int batch_size)
{
    if (!M_learning_enabled)
    {
        return 0.0;
    }

    if (!M_experience_buffer->hasEnoughSamples(batch_size))
    {
        dlog.addText(Logger::TEAM,
                     __FILE__": (updateFromExperience) Not enough samples (need %d, have %zu)",
                     batch_size, M_experience_buffer->size());
        return 0.0;
    }

    // Sample batch from experience buffer
    RLExperienceBuffer::ExperienceBatch batch = M_experience_buffer->sampleBatch(batch_size);

    if (batch.size() == 0)
    {
        return 0.0;
    }

    // TD-learning update
    // For DQN: target = reward + gamma * V(next_state)
    // For Actor-Critic: use advantage = target - V(state)

    double total_loss = 0.0;
    const double gamma = 0.99;  // Discount factor

    for (size_t i = 0; i < batch.size(); ++i)
    {
        // Get current value estimate
        double current_value = predictValue(batch.states[i]);

        // Compute TD target
        double next_value = 0.0;
        if (!batch.terminals[i])
        {
            next_value = predictValue(batch.next_states[i]);
        }

        double td_target = batch.rewards[i] + gamma * next_value;

        // Compute TD error (loss)
        double td_error = td_target - current_value;
        total_loss += std::abs(td_error);

        // Note: Actual weight update would be done here
        // For offline training, this would update network weights
        // For now, we just compute the loss for monitoring
    }

    double avg_loss = total_loss / batch.size();

    dlog.addText(Logger::TEAM,
                 __FILE__": (updateFromExperience) batch_size=%zu, avg_loss=%.4f",
                 batch.size(), avg_loss);

    return avg_loss;
}

/*-------------------------------------------------------------------*/
void
RLFieldEvaluator::storeExperience(const std::vector<double> & state,
                                   int action,
                                   double reward,
                                   const std::vector<double> & next_state,
                                   bool terminal,
                                   int cycle,
                                   int unum)
{
    if (!M_learning_enabled)
    {
        return;
    }

    RLExperienceBuffer::Experience exp;
    exp.state_features = state;
    exp.action_index = action;
    exp.reward = reward;
    exp.next_state_features = next_state;
    exp.is_terminal = terminal;
    exp.cycle = cycle;
    exp.player_unum = unum;

    M_experience_buffer->addExperience(exp);
}

/*-------------------------------------------------------------------*/
bool
RLFieldEvaluator::loadWeights(const std::string & value_path,
                               const std::string & policy_path)
{
    M_value_weights_path = value_path;
    M_policy_weights_path = policy_path;

    // Load value network weights
    if (!value_path.empty())
    {
        std::ifstream fin(value_path);
        if (fin.is_open())
        {
            // Read weights file in CppDNN format
            // Format similar to unmark_dnn_weights.txt
            dlog.addText(Logger::TEAM,
                         __FILE__": (loadWeights) Loading value network from %s",
                         value_path.c_str());

            // Note: Actual loading would use CppDNN API
            // M_value_network.Read(value_path);
            fin.close();
        }
        else
        {
            dlog.addText(Logger::TEAM,
                         __FILE__": (loadWeights) Failed to open %s",
                         value_path.c_str());
        }
    }

    // Load policy network weights
    if (!policy_path.empty())
    {
        std::ifstream fin(policy_path);
        if (fin.is_open())
        {
            dlog.addText(Logger::TEAM,
                         __FILE__": (loadWeights) Loading policy network from %s",
                         policy_path.c_str());
            fin.close();
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
bool
RLFieldEvaluator::saveWeights(const std::string & value_path,
                               const std::string & policy_path)
{
    // Save value network weights
    if (!value_path.empty())
    {
        dlog.addText(Logger::TEAM,
                     __FILE__": (saveWeights) Saving value network to %s",
                     value_path.c_str());
        // Note: Actual saving would use CppDNN API
        // M_value_network.Write(value_path);
    }

    // Save policy network weights
    if (!policy_path.empty())
    {
        dlog.addText(Logger::TEAM,
                     __FILE__": (saveWeights) Saving policy network to %s",
                     policy_path.c_str());
    }

    return true;
}

/*-------------------------------------------------------------------*/
double
RLFieldEvaluator::getAverageValue() const
{
    if (M_total_predictions == 0)
        return 0.0;
    return M_total_value / M_total_predictions;
}

/*-------------------------------------------------------------------*/
void
RLFieldEvaluator::resetStats()
{
    M_total_predictions = 0;
    M_total_value = 0.0;
}
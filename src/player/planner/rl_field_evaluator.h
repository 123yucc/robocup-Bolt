// -*-c++-*-
/*
 * Bolt Team - RoboCup 2D 2026
 * Reinforcement Learning Field Evaluator
 *
 * Copyright: Wuhan University - Bolt Team
 * Strategy: Use SampleFieldEvaluator as base, add RL enhancement when trained
 */

#ifndef RCSC_PLAYER_RL_FIELD_EVALUATOR_H
#define RCSC_PLAYER_RL_FIELD_EVALUATOR_H

#include "field_evaluator.h"
#include "rl_reward_calculator.h"
#include "rl_experience_buffer.h"
#include <CppDNN/DeepNueralNetwork.h>

#include <memory>
#include <string>

/*!
  \class RLFieldEvaluator
  \brief RL-enhanced field evaluator

  Uses original SampleFieldEvaluator as base (proven logic)
  Adds RL enhancement only when trained weights are available
*/
class RLFieldEvaluator : public FieldEvaluator {
public:
    typedef std::shared_ptr<RLFieldEvaluator> Ptr;
    typedef std::shared_ptr<const RLFieldEvaluator> ConstPtr;

    // Action categories for RL
    enum ActionType {
        PASS = 0,
        SHOOT = 1,
        DRIBBLE = 2,
        HOLD = 3,
        NUM_ACTIONS = 4
    };

private:
    // Base evaluator - original SampleFieldEvaluator (proven logic)
    std::shared_ptr<FieldEvaluator> M_base_evaluator;

    // Neural networks (for future DNN integration)
    DeepNueralNetwork M_value_network;
    DeepNueralNetwork M_policy_network;

    // RL components
    RLRewardCalculator M_reward_calculator;
    RLExperienceBuffer::Ptr M_experience_buffer;

    // Configuration
    int M_state_dim;
    bool M_learning_enabled;
    double M_alpha;  // Weight: 0.9 = 90% base evaluator, 10% RL enhancement
    bool M_weights_loaded;  // True only if trained weights loaded successfully

    // Statistics (mutable for const methods)
    mutable int M_total_predictions;
    mutable double M_total_value;

    // State tracking
    std::vector<double> M_prev_state_features;
    int M_prev_action;

    // Network paths
    std::string M_value_weights_path;
    std::string M_policy_weights_path;

public:
    /*!
      \brief Constructor
      \param state_dim State feature dimension (default 350)
    */
    explicit RLFieldEvaluator(int state_dim = 350);

    /*!
      \brief Destructor
    */
    ~RLFieldEvaluator();

    /*!
      \brief Evaluation function (override from FieldEvaluator)
      \param state Predicted state
      \param path Action path
      \param wm World model
      \return Evaluation value (base + RL enhancement)
    */
    double operator()(const PredictState & state,
                      const std::vector<ActionStatePair> & path,
                      const rcsc::WorldModel & wm) const override;

    /*!
      \brief Predict state value using DNN
      \param features State features vector
      \return Predicted value
    */
    double predictValue(const std::vector<double> & features) const;

    /*!
      \brief Predict action probabilities using policy network
      \param features State features vector
      \return Vector of action probabilities
    */
    std::vector<double> predictPolicy(const std::vector<double> & features) const;

    /*!
      \brief Get best action from policy
      \param features State features vector
      \return Best action index
    */
    int getBestAction(const std::vector<double> & features) const;

    /*!
      \brief Update networks from experience (TD-learning)
      \param batch_size Number of samples
      \return Average loss
    */
    double updateFromExperience(int batch_size = 64);

    /*!
      \brief Store experience for later training
    */
    void storeExperience(const std::vector<double> & state,
                         int action,
                         double reward,
                         const std::vector<double> & next_state,
                         bool terminal,
                         int cycle,
                         int unum);

    /*!
      \brief Enable/disable learning mode
    */
    void setLearningEnabled(bool enabled) { M_learning_enabled = enabled; }
    bool isLearningEnabled() const { return M_learning_enabled; }

    /*!
      \brief Set alpha (base vs RL weight)
    */
    void setAlpha(double alpha) { M_alpha = alpha; }
    double getAlpha() const { return M_alpha; }

    /*!
      \brief Check if weights are loaded
    */
    bool hasWeightsLoaded() const { return M_weights_loaded; }

    /*!
      \brief Load network weights from files
    */
    bool loadWeights(const std::string & value_path,
                     const std::string & policy_path);

    /*!
      \brief Save network weights to files
    */
    bool saveWeights(const std::string & value_path,
                     const std::string & policy_path);

    /*!
      \brief Get reward calculator
    */
    RLRewardCalculator & rewardCalculator() { return M_reward_calculator; }

    /*!
      \brief Get experience buffer
    */
    RLExperienceBuffer::Ptr experienceBuffer() { return M_experience_buffer; }

    /*!
      \brief Get statistics
    */
    int getTotalPredictions() const { return M_total_predictions; }
    double getAverageValue() const;

    /*!
      \brief Reset statistics
    */
    void resetStats();
};

#endif // RCSC_PLAYER_RL_FIELD_EVALUATOR_H
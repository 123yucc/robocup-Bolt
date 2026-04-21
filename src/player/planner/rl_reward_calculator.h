// -*-c++-*-
/*
 * Bolt Team - RoboCup 2D 2026
 * Reinforcement Learning Reward Calculator
 *
 * Copyright: Wuhan University - Bolt Team
 * Implements reward function for DQN/PPO training
 */

#ifndef RCSC_PLAYER_RL_REWARD_CALCULATOR_H
#define RCSC_PLAYER_RL_REWARD_CALCULATOR_H

#include <rcsc/player/world_model.h>
#include <rcsc/geom/vector_2d.h>
#include <rcsc/common/server_param.h>

/*!
  \class RLRewardCalculator
  \brief Calculates reward for reinforcement learning

  Reward design based on:
  - Goal scoring/conceding
  - Ball possession
  - Ball advancement
  - Pass success
  - Defensive pressure
  - Positional advantage
*/
class RLRewardCalculator {
public:
    /*!
      \brief Reward components structure
    */
    struct RewardComponents {
        double goal_reward;           // +1000 (score goal)
        double goal_conceded;         // -1000 (concede goal)
        double possession_gain;       // +50 (gain possession)
        double possession_loss;       // -30 (lose possession)
        double pass_success;          // +30 (successful pass)
        double ball_advancement;      // +5 * x_change (ball moved forward)
        double defensive_pressure;    // -20 (ball near our goal)
        double positional_advantage;  // Voronoi-based reward
        double total_reward;
    };

    static const double GOAL_REWARD;
    static const double GOAL_CONCEDED_PENALTY;
    static const double POSSESSION_GAIN_REWARD;
    static const double POSSESSION_LOSS_PENALTY;
    static const double PASS_SUCCESS_REWARD;
    static const double ADVANCEMENT_FACTOR;
    static const double DEFENSIVE_PRESSURE_PENALTY;
    static const double DEFENSIVE_ZONE_X;

private:
    // Previous state tracking (mutable for const methods)
    mutable bool M_prev_had_possession;
    mutable rcsc::Vector2D M_prev_ball_pos;
    mutable int M_prev_our_score;
    mutable int M_prev_opp_score;
    mutable int M_prev_cycle;

public:
    /*!
      \brief Constructor
    */
    RLRewardCalculator();

    /*!
      \brief Reset state tracking (for new match)
    */
    void reset();

    /*!
      \brief Calculate reward from state transition
      \param wm_before WorldModel before action
      \param wm_after WorldModel after action
      \return Total reward value
    */
    double calculateReward(const rcsc::WorldModel & wm_before,
                          const rcsc::WorldModel & wm_after) const;

    /*!
      \brief Get detailed reward breakdown
      \param wm_before WorldModel before action
      \param wm_after WorldModel after action
      \return RewardComponents structure
    */
    RewardComponents calculateDetailedReward(const rcsc::WorldModel & wm_before,
                                             const rcsc::WorldModel & wm_after) const;

    /*!
      \brief Check if goal was scored
      \param wm WorldModel
      \return true if ball in opponent goal area
    */
    bool checkGoalScored(const rcsc::WorldModel & wm) const;

    /*!
      \brief Check if goal was conceded
      \param wm WorldModel
      \return true if ball in our goal area
    */
    bool checkGoalConceded(const rcsc::WorldModel & wm) const;

    /*!
      \brief Check ball possession status
      \param wm WorldModel
      \return true if we have possession
    */
    bool hasPossession(const rcsc::WorldModel & wm) const;

    /*!
      \brief Calculate ball advancement reward
      \param before_x Ball x before
      \param after_x Ball x after
      \return advancement reward
    */
    double calculateAdvancement(double before_x, double after_x) const;

    /*!
      \brief Calculate defensive pressure penalty
      \param wm WorldModel
      \return pressure penalty
    */
    double calculateDefensivePressure(const rcsc::WorldModel & wm) const;

    /*!
      \brief Estimate positional advantage using Voronoi
      \param wm WorldModel
      \return positional advantage score
    */
    double calculatePositionalAdvantage(const rcsc::WorldModel & wm) const;
};

#endif // RCSC_PLAYER_RL_REWARD_CALCULATOR_H
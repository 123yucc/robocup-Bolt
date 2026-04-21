// -*-c++-*-
/*
 * Bolt Team - RoboCup 2D 2026
 * Reinforcement Learning Reward Calculator Implementation
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rl_reward_calculator.h"

#include <rcsc/player/intercept_table.h>
#include <rcsc/common/logger.h>
#include <rcsc/geom/voronoi_diagram.h>
#include <cmath>

using namespace rcsc;

// Reward constants
const double RLRewardCalculator::GOAL_REWARD = 1000.0;
const double RLRewardCalculator::GOAL_CONCEDED_PENALTY = -1000.0;
const double RLRewardCalculator::POSSESSION_GAIN_REWARD = 50.0;
const double RLRewardCalculator::POSSESSION_LOSS_PENALTY = -30.0;
const double RLRewardCalculator::PASS_SUCCESS_REWARD = 30.0;
const double RLRewardCalculator::ADVANCEMENT_FACTOR = 5.0;
const double RLRewardCalculator::DEFENSIVE_PRESSURE_PENALTY = -20.0;
const double RLRewardCalculator::DEFENSIVE_ZONE_X = -35.0;

/*-------------------------------------------------------------------*/
RLRewardCalculator::RLRewardCalculator()
    : M_prev_had_possession(false)
    , M_prev_ball_pos(Vector2D::INVALIDATED)
    , M_prev_our_score(0)
    , M_prev_opp_score(0)
    , M_prev_cycle(0)
{
}

/*-------------------------------------------------------------------*/
void
RLRewardCalculator::reset()
{
    M_prev_had_possession = false;
    M_prev_ball_pos = Vector2D::INVALIDATED;
    M_prev_our_score = 0;
    M_prev_opp_score = 0;
    M_prev_cycle = 0;
}

/*-------------------------------------------------------------------*/
bool
RLRewardCalculator::checkGoalScored(const WorldModel & wm) const
{
    const ServerParam & SP = ServerParam::i();
    const Vector2D & ball_pos = wm.ball().pos();

    // Check if ball is in opponent goal area
    if (ball_pos.x > SP.theirTeamGoalPos().x - 0.1 &&
        std::abs(ball_pos.y) < SP.goalHalfWidth() + 0.1)
    {
        return true;
    }
    return false;
}

/*-------------------------------------------------------------------*/
bool
RLRewardCalculator::checkGoalConceded(const WorldModel & wm) const
{
    const ServerParam & SP = ServerParam::i();
    const Vector2D & ball_pos = wm.ball().pos();

    // Check if ball is in our goal area
    if (ball_pos.x < SP.ourTeamGoalPos().x + 0.1 &&
        std::abs(ball_pos.y) < SP.goalHalfWidth() + 0.1)
    {
        return true;
    }
    return false;
}

/*-------------------------------------------------------------------*/
bool
RLRewardCalculator::hasPossession(const WorldModel & wm) const
{
    // We have possession if:
    // 1. A teammate is the fastest to ball
    // 2. Ball is kickable by a teammate

    const InterceptTable & intercept = wm.interceptTable();

    int tm_step = intercept.teammateStep();
    int opp_step = intercept.opponentStep();

    // Teammate reaches ball first
    if (tm_step <= opp_step && tm_step < 100)
    {
        return true;
    }

    // Check if any teammate can kick
    // Note: AbstractPlayerObject doesn't have isKickable, estimate from distFromBall
    const double kickable_margin = 1.0;  // Approximate kickable distance

    for (int i = 1; i <= 11; ++i)
    {
        const AbstractPlayerObject * tm = wm.ourPlayer(i);
        if (tm && tm->distFromBall() < kickable_margin)
        {
            return true;
        }
    }

    return false;
}

/*-------------------------------------------------------------------*/
double
RLRewardCalculator::calculateAdvancement(double before_x, double after_x) const
{
    // Positive advancement means ball moved toward opponent goal
    double advancement = after_x - before_x;

    // Scale advancement: more reward for forward movement
    // Less reward (or penalty) for backward movement
    if (advancement > 0)
    {
        return advancement * ADVANCEMENT_FACTOR;
    }
    else
    {
        // Backward movement penalty (half the advancement factor)
        return advancement * ADVANCEMENT_FACTOR * 0.5;
    }
}

/*-------------------------------------------------------------------*/
double
RLRewardCalculator::calculateDefensivePressure(const WorldModel & wm) const
{
    const Vector2D & ball_pos = wm.ball().pos();

    // Defensive pressure increases as ball approaches our goal
    if (ball_pos.x < DEFENSIVE_ZONE_X)
    {
        // Scale penalty based on how close ball is to our goal
        double danger_factor = (DEFENSIVE_ZONE_X - ball_pos.x) / DEFENSIVE_ZONE_X;
        return DEFENSIVE_PRESSURE_PENALTY * danger_factor;
    }

    return 0.0;
}

/*-------------------------------------------------------------------*/
double
RLRewardCalculator::calculatePositionalAdvantage(const WorldModel & wm) const
{
    // Simple positional advantage: count teammates in good positions
    // Good positions: ahead of ball, near opponent goal, in open space

    double advantage = 0.0;
    const Vector2D & ball_pos = wm.ball().pos();

    for (int i = 1; i <= 11; ++i)
    {
        const AbstractPlayerObject * tm = wm.ourPlayer(i);
        if (!tm)
            continue;

        // Skip goalie
        if (i == 1)
            continue;

        const Vector2D & tm_pos = tm->pos();

        // Reward for being ahead of ball (attacking position)
        if (tm_pos.x > ball_pos.x)
        {
            advantage += 1.0;
        }

        // Extra reward for being in opponent half
        if (tm_pos.x > 0)
        {
            advantage += 0.5;
        }
    }

    return advantage;
}

/*-------------------------------------------------------------------*/
double
RLRewardCalculator::calculateReward(const WorldModel & wm_before,
                                    const WorldModel & wm_after) const
{
    RewardComponents components = calculateDetailedReward(wm_before, wm_after);
    return components.total_reward;
}

/*-------------------------------------------------------------------*/
RLRewardCalculator::RewardComponents
RLRewardCalculator::calculateDetailedReward(const WorldModel & wm_before,
                                            const WorldModel & wm_after) const
{
    RewardComponents rewards;
    rewards.goal_reward = 0.0;
    rewards.goal_conceded = 0.0;
    rewards.possession_gain = 0.0;
    rewards.possession_loss = 0.0;
    rewards.pass_success = 0.0;
    rewards.ball_advancement = 0.0;
    rewards.defensive_pressure = 0.0;
    rewards.positional_advantage = 0.0;
    rewards.total_reward = 0.0;

    // 1. Goal detection
    if (checkGoalScored(wm_after))
    {
        rewards.goal_reward = GOAL_REWARD;
        dlog.addText(rcsc::Logger::TEAM,
                     __FILE__": (calculateDetailedReward) GOAL SCORED! reward=%.1f",
                     rewards.goal_reward);
    }

    if (checkGoalConceded(wm_after))
    {
        rewards.goal_conceded = GOAL_CONCEDED_PENALTY;
        dlog.addText(rcsc::Logger::TEAM,
                     __FILE__": (calculateDetailedReward) GOAL CONCEDED! penalty=%.1f",
                     rewards.goal_conceded);
    }

    // 2. Possession change detection
    bool now_has_possession = hasPossession(wm_after);

    if (!M_prev_had_possession && now_has_possession)
    {
        rewards.possession_gain = POSSESSION_GAIN_REWARD;
        dlog.addText(rcsc::Logger::TEAM,
                     __FILE__": (calculateDetailedReward) GAINED POSSESSION! reward=%.1f",
                     rewards.possession_gain);
    }
    else if (M_prev_had_possession && !now_has_possession)
    {
        rewards.possession_loss = POSSESSION_LOSS_PENALTY;
        dlog.addText(rcsc::Logger::TEAM,
                     __FILE__": (calculateDetailedReward) LOST POSSESSION! penalty=%.1f",
                     rewards.possession_loss);
    }

    M_prev_had_possession = now_has_possession;

    // 3. Ball advancement
    if (M_prev_ball_pos.isValid())
    {
        rewards.ball_advancement = calculateAdvancement(M_prev_ball_pos.x,
                                                        wm_after.ball().pos().x);
    }
    M_prev_ball_pos = wm_after.ball().pos();

    // 4. Defensive pressure
    rewards.defensive_pressure = calculateDefensivePressure(wm_after);

    // 5. Positional advantage
    rewards.positional_advantage = calculatePositionalAdvantage(wm_after);

    // Calculate total reward
    rewards.total_reward = rewards.goal_reward
                          + rewards.goal_conceded
                          + rewards.possession_gain
                          + rewards.possession_loss
                          + rewards.pass_success
                          + rewards.ball_advancement
                          + rewards.defensive_pressure
                          + rewards.positional_advantage;

    // Log total reward periodically
    if (wm_after.time().cycle() % 100 == 0)
    {
        dlog.addText(rcsc::Logger::TEAM,
                     __FILE__": (calculateDetailedReward) cycle=%d total_reward=%.1f",
                     wm_after.time().cycle(), rewards.total_reward);
    }

    return rewards;
}
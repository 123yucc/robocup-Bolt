#include "bolt_pass_inference.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/player/player_object.h>
#include <rcsc/types.h>

#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <iostream>

using namespace rcsc;

BoltPassInference& BoltPassInference::getInstance() {
    static BoltPassInference instance;
    return instance;
}

void BoltPassInference::tryLoad(const std::string& weight_path) {
    if (!m_loaded) {
        m_loaded = m_dnn.ReadFromKeras(weight_path);
        if (m_loaded)
            std::cerr << "[BoltPass] loaded: " << weight_path << std::endl;
        else
            std::cerr << "[BoltPass] WARNING: failed to load " << weight_path << std::endl;
    }
}

bool BoltPassInference::isLoaded() const { return m_loaded; }

static double sdist(double x1, double y1, double x2, double y2) {
    double dx = x1 - x2, dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

// Calculate interception probability along pass path
static double passInterceptionProb(
        double px, double py, double rx, double ry,
        const PlayerObject::Cont& opponents) {
    double dx = rx - px, dy = ry - py;
    double pass_len = std::sqrt(dx * dx + dy * dy);
    if (pass_len < 1e-6) return 0.0;

    int interceptors = 0;
    double min_dist_to_path = 999.0;

    for (const AbstractPlayerObject* opp : opponents) {
        if (!opp || !opp->pos().isValid()) continue;

        double ex = opp->pos().x - px, ey = opp->pos().y - py;
        double t = std::max(0.0, std::min(1.0, (ex * dx + ey * dy) / (pass_len * pass_len)));
        double proj_x = px + t * dx, proj_y = py + t * dy;
        double dist = sdist(opp->pos().x, opp->pos().y, proj_x, proj_y);

        if (dist < min_dist_to_path) min_dist_to_path = dist;
        if (dist < 1.5) ++interceptors;
    }

    return std::min(1.0, interceptors / 3.0);
}

double BoltPassInference::score(const WorldModel& wm,
                                const Vector2D& passer_pos,
                                const Vector2D& receiver_pos,
                                const Vector2D& ball_pos) {
    if (!m_loaded) {
        return heuristicScore(wm, passer_pos, receiver_pos, ball_pos);
    }

    try {

    static const double FIELD_LEN  = 105.0;
    static const double FIELD_WID  =  68.0;
    static const double HALF_LEN   = FIELD_LEN / 2.0;
    static const double HALF_WID   = FIELD_WID  / 2.0;

    // Passer state (7 features)
    double p_x = passer_pos.x;
    double p_y = passer_pos.y;
    double p_vx = wm.self().vel().x;
    double p_vy = wm.self().vel().y;
    double p_stamina = std::min(1.0, wm.self().stamina() / 8000.0);
    double p_body = wm.self().body().degree();

    // Receiver state (8 features)
    double r_x = receiver_pos.x;
    double r_y = receiver_pos.y;
    double r_vx = 0.0, r_vy = 0.0, r_stamina = 0.7;
    bool r_offside = false;

    // Try to get receiver velocity and stamina
    for (const AbstractPlayerObject* tm : wm.teammates()) {
        if (tm && tm->pos().isValid() && tm->pos().dist(receiver_pos) < 2.0) {
            r_vx = tm->vel().x;
            r_vy = tm->vel().y;
            // stamina not available in AbstractPlayerObject, use default
            break;
        }
    }

    // Check offside
    if (receiver_pos.x > wm.offsideLineX()) r_offside = true;

    // Ball state (4 features)
    double b_x = ball_pos.x;
    double b_y = ball_pos.y;
    double b_vx = wm.ball().vel().x;
    double b_vy = wm.ball().vel().y;

    // Pass parameters (4 features)
    double pass_dist = sdist(p_x, p_y, r_x, r_y);
    double pass_angle = std::atan2(r_y - p_y, r_x - p_x);
    double pass_power = std::min(1.0, pass_dist / 40.0); // normalized by max pass distance
    double body_angle_diff = std::abs(AngleDeg::normalize_angle((pass_angle * 180.0 / M_PI) - p_body));

    // Opponent threat (5 features)
    double nearest_opp_dist = 999.0;
    double nearest_opp_to_receiver = 999.0;
    int opps_near_path = 0;

    for (const AbstractPlayerObject* opp : wm.opponents()) {
        if (!opp || !opp->pos().isValid()) continue;

        double d_passer = sdist(p_x, p_y, opp->pos().x, opp->pos().y);
        if (d_passer < nearest_opp_dist) nearest_opp_dist = d_passer;

        double d_receiver = sdist(r_x, r_y, opp->pos().x, opp->pos().y);
        if (d_receiver < nearest_opp_to_receiver) nearest_opp_to_receiver = d_receiver;

        // Check if opponent is near pass path
        double ex = opp->pos().x - p_x, ey = opp->pos().y - p_y;
        double dx = r_x - p_x, dy = r_y - p_y;
        double t = std::max(0.0, std::min(1.0, (ex * dx + ey * dy) / (pass_dist * pass_dist + 1e-6)));
        double proj_x = p_x + t * dx, proj_y = p_y + t * dy;
        if (sdist(opp->pos().x, opp->pos().y, proj_x, proj_y) < 3.0) ++opps_near_path;
    }

    double interception_prob = passInterceptionProb(p_x, p_y, r_x, r_y, wm.opponents());

    // Tactical value (7 features)
    double forward_progress = (r_x - p_x) / HALF_LEN; // positive if forward pass
    double receiver_goal_dist = sdist(r_x, r_y, 52.5, 0.0);

    // Estimate shooting opportunity after receiving
    double shoot_angle = 0.0;
    if (r_x > 30.0) {
        double top_ang = std::atan2(7.01 - r_y, 52.5 - r_x);
        double bot_ang = std::atan2(-7.01 - r_y, 52.5 - r_x);
        shoot_angle = std::abs(top_ang - bot_ang) * 180.0 / M_PI;
    }

    // Game context
    int score_l = wm.gameMode().scoreLeft();
    int score_r = wm.gameMode().scoreRight();
    double score_diff = (wm.ourSide() == LEFT)
                        ? (score_l - score_r) / 5.0
                        : (score_r - score_l) / 5.0;
    double cycle_norm = static_cast<double>(wm.time().cycle()) / 6000.0;

    // Build 35-dim feature vector
    MatrixXd input(35, 1);

    // Passer state (0-6)
    input( 0, 0) = p_x / HALF_LEN;
    input( 1, 0) = p_y / HALF_WID;
    input( 2, 0) = p_vx;
    input( 3, 0) = p_vy;
    input( 4, 0) = p_stamina;
    input( 5, 0) = std::sin(p_body * M_PI / 180.0);
    input( 6, 0) = std::cos(p_body * M_PI / 180.0);

    // Receiver state (7-14)
    input( 7, 0) = r_x / HALF_LEN;
    input( 8, 0) = r_y / HALF_WID;
    input( 9, 0) = r_vx;
    input(10, 0) = r_vy;
    input(11, 0) = r_stamina;
    input(12, 0) = r_offside ? 1.0 : 0.0;
    input(13, 0) = (r_x - wm.offsideLineX()) / HALF_LEN;
    input(14, 0) = nearest_opp_to_receiver / FIELD_LEN;

    // Ball state (15-18)
    input(15, 0) = b_x / HALF_LEN;
    input(16, 0) = b_y / HALF_WID;
    input(17, 0) = b_vx;
    input(18, 0) = b_vy;

    // Pass parameters (19-22)
    input(19, 0) = pass_dist / FIELD_LEN;
    input(20, 0) = std::sin(pass_angle);
    input(21, 0) = std::cos(pass_angle);
    input(22, 0) = body_angle_diff / 180.0;

    // Opponent threat (23-27)
    input(23, 0) = nearest_opp_dist / FIELD_LEN;
    input(24, 0) = interception_prob;
    input(25, 0) = opps_near_path / 5.0;
    input(26, 0) = pass_power;
    input(27, 0) = (pass_dist < 15.0) ? 1.0 : 0.0; // short pass flag

    // Tactical value (28-34)
    input(28, 0) = forward_progress;
    input(29, 0) = receiver_goal_dist / FIELD_LEN;
    input(30, 0) = shoot_angle / 180.0;
    input(31, 0) = (r_x > 36.0 && std::abs(r_y) < 20.0) ? 1.0 : 0.0; // in penalty area
    input(32, 0) = score_diff;
    input(33, 0) = cycle_norm;
    input(34, 0) = (wm.gameMode().type() == GameMode::PlayOn) ? 1.0 : 0.0;

    m_dnn.Calculate(input);
    return m_dnn.mOutput(0, 0);
    } catch (const std::exception& e) {
        std::cerr << "[BoltPass] ML inference failed: " << e.what() << ", using heuristic" << std::endl;
        return heuristicScore(wm, passer_pos, receiver_pos, ball_pos);
    }
}

double BoltPassInference::heuristicScore(const WorldModel& wm,
                                         const Vector2D& passer_pos,
                                         const Vector2D& receiver_pos,
                                         const Vector2D& ball_pos) {
    static const double FIELD_LEN = 105.0;

    double p_x = passer_pos.x, p_y = passer_pos.y;
    double r_x = receiver_pos.x, r_y = receiver_pos.y;

    // Pass distance
    double dx = r_x - p_x, dy = r_y - p_y;
    double pass_dist = std::sqrt(dx * dx + dy * dy);

    // Forward progress
    double forward_progress = (r_x - p_x);

    // Nearest opponent to receiver
    double nearest_opp_to_receiver = 999.0;
    int opps_near_path = 0;

    for (const AbstractPlayerObject* opp : wm.opponents()) {
        if (!opp || !opp->pos().isValid()) continue;

        double d_receiver = std::sqrt((r_x - opp->pos().x) * (r_x - opp->pos().x) +
                                      (r_y - opp->pos().y) * (r_y - opp->pos().y));
        if (d_receiver < nearest_opp_to_receiver) nearest_opp_to_receiver = d_receiver;

        // Check if opponent is near pass path
        double ex = opp->pos().x - p_x, ey = opp->pos().y - p_y;
        double t = std::max(0.0, std::min(1.0, (ex * dx + ey * dy) / (pass_dist * pass_dist + 1e-6)));
        double proj_x = p_x + t * dx, proj_y = p_y + t * dy;
        double dist_to_path = std::sqrt((opp->pos().x - proj_x) * (opp->pos().x - proj_x) +
                                        (opp->pos().y - proj_y) * (opp->pos().y - proj_y));
        if (dist_to_path < 3.0) ++opps_near_path;
    }

    // Heuristic score: favor forward passes, clear paths, receivers far from opponents
    double score = 0.5; // baseline
    score += 0.2 * std::max(0.0, std::min(1.0, forward_progress / 20.0)); // forward progress bonus
    score += 0.2 * std::min(1.0, nearest_opp_to_receiver / 5.0); // receiver space bonus
    score -= 0.15 * std::min(1.0, opps_near_path / 3.0); // path blocking penalty
    score += 0.15 * std::max(0.0, 1.0 - pass_dist / 30.0); // reasonable distance bonus

    return std::max(0.0, std::min(1.0, score));
}

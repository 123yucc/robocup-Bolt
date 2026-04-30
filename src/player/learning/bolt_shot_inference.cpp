#include "bolt_shot_inference.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/types.h>

#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <iostream>

using namespace rcsc;

BoltShotInference& BoltShotInference::getInstance() {
    static BoltShotInference instance;
    return instance;
}

void BoltShotInference::tryLoad(const std::string& weight_path) {
    if (!m_loaded) {
        m_loaded = m_dnn.ReadFromKeras(weight_path);
        if (m_loaded)
            std::cerr << "[BoltShot] loaded: " << weight_path << std::endl;
        else
            std::cerr << "[BoltShot] WARNING: failed to load " << weight_path << std::endl;
    }
}

bool BoltShotInference::isLoaded() const { return m_loaded; }

static double sdist(double x1, double y1, double x2, double y2) {
    double dx = x1 - x2, dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

static std::pair<double, int> pathBlocked(
        double sx, double sy, double tx, double ty,
        const AbstractPlayerObject::Cont& defenders) {
    double dx = tx - sx, dy = ty - sy;
    double path_len = std::sqrt(dx * dx + dy * dy);
    if (path_len < 1e-6) return {0.0, 0};

    int blockers = 0;
    for (const AbstractPlayerObject* d : defenders) {
        if (!d || !d->pos().isValid()) continue;
        double ex = d->pos().x - sx, ey = d->pos().y - sy;
        double t  = std::max(0.0, std::min(1.0, (ex * dx + ey * dy) / (path_len * path_len)));
        double px = sx + t * dx, py = sy + t * dy;
        if (sdist(d->pos().x, d->pos().y, px, py) < 0.6) ++blockers;
    }
    return {std::min(1.0, blockers / 3.0), blockers};
}

double BoltShotInference::score(const WorldModel& wm,
                                const Vector2D& shooter_pos,
                                const Vector2D& ball_pos,
                                double target_y) {
    if (!m_loaded) {
        return heuristicScore(wm, shooter_pos, ball_pos, target_y);
    }

    try {

    static const double FIELD_LEN  = 105.0;
    static const double FIELD_WID  =  68.0;
    static const double HALF_LEN   = FIELD_LEN / 2.0;
    static const double HALF_WID   = FIELD_WID  / 2.0;
    static const double GOAL_Y_TOP =  7.01;
    static const double GOAL_Y_BOT = -7.01;
    static const int    NUM_GRIDS  = 24;

    double sx = shooter_pos.x, sy = shooter_pos.y;
    double target_x = (wm.ourSide() == LEFT) ? 52.5 : -52.5;

    double s_vx  = wm.self().vel().x;
    double s_vy  = wm.self().vel().y;
    double s_stam = std::min(1.0, wm.self().stamina() / 8000.0);
    double s_body = wm.self().body().degree();

    double shot_dist  = sdist(sx, sy, target_x, target_y);
    double shot_angle = std::atan2(target_y - sy, target_x - sx);
    double top_ang    = std::atan2(GOAL_Y_TOP - sy, target_x - sx);
    double bot_ang    = std::atan2(GOAL_Y_BOT - sy, target_x - sx);
    double open_angle = std::abs(top_ang - bot_ang) * 180.0 / M_PI;

    double grid_step  = (GOAL_Y_TOP - GOAL_Y_BOT) / NUM_GRIDS;
    int    tgt_idx    = 0;
    double best_diff  = std::abs(GOAL_Y_BOT + 0.5 * grid_step - target_y);
    for (int i = 1; i < NUM_GRIDS; ++i) {
        double cy = GOAL_Y_BOT + (i + 0.5) * grid_step;
        double d  = std::abs(cy - target_y);
        if (d < best_diff) { best_diff = d; tgt_idx = i; }
    }
    double tgt_idx_norm = static_cast<double>(tgt_idx) / (NUM_GRIDS - 1);

    double gx = target_x, gy = 0.0, gvx = 0.0, gvy = 0.0;
    double dist_goalie = sdist(gx, gy, target_x, target_y);
    for (const AbstractPlayerObject* opp : wm.theirPlayers()) {
        if (opp && opp->unum() == 1 && opp->pos().isValid()) {
            gx = opp->pos().x; gy = opp->pos().y;
            gvx = opp->vel().x; gvy = opp->vel().y;
            dist_goalie = sdist(gx, gy, target_x, target_y);
            break;
        }
    }

    double nd_x = target_x, nd_y = 0.0, dist_nd = 99.0;
    for (const AbstractPlayerObject* opp : wm.theirPlayers()) {
        if (!opp || !opp->pos().isValid() || opp->unum() == 1) continue;
        double d = sdist(sx, sy, opp->pos().x, opp->pos().y);
        if (d < dist_nd) { dist_nd = d; nd_x = opp->pos().x; nd_y = opp->pos().y; }
    }

    auto [path_blocked, num_blockers] = pathBlocked(sx, sy, target_x, target_y,
                                                     wm.theirPlayers());

    int score_l = wm.gameMode().scoreLeft();
    int score_r = wm.gameMode().scoreRight();
    double score_diff = (wm.ourSide() == LEFT)
                        ? (score_l - score_r) / 5.0
                        : (score_r - score_l) / 5.0;
    double cycle_norm = static_cast<double>(wm.time().cycle()) / 6000.0;

    // Build 27-dim feature vector (matches ml/features/shot_target_features.py)
    MatrixXd input(27, 1);
    input( 0, 0) = sx              / HALF_LEN;
    input( 1, 0) = sy              / HALF_WID;
    input( 2, 0) = s_vx;
    input( 3, 0) = s_vy;
    input( 4, 0) = s_stam;
    input( 5, 0) = std::sin(s_body * M_PI / 180.0);
    input( 6, 0) = std::cos(s_body * M_PI / 180.0);
    input( 7, 0) = ball_pos.x      / HALF_LEN;
    input( 8, 0) = ball_pos.y      / HALF_WID;
    input( 9, 0) = target_y        / GOAL_Y_TOP;
    input(10, 0) = tgt_idx_norm;
    input(11, 0) = shot_dist       / FIELD_LEN;
    input(12, 0) = std::sin(shot_angle);
    input(13, 0) = std::cos(shot_angle);
    input(14, 0) = open_angle      / 180.0;
    input(15, 0) = gx              / HALF_LEN;
    input(16, 0) = gy              / HALF_WID;
    input(17, 0) = gvx;
    input(18, 0) = gvy;
    input(19, 0) = dist_goalie     / FIELD_WID;
    input(20, 0) = nd_x            / HALF_LEN;
    input(21, 0) = nd_y            / HALF_WID;
    input(22, 0) = dist_nd         / FIELD_LEN;
    input(23, 0) = path_blocked;
    input(24, 0) = num_blockers    / 5.0;
    input(25, 0) = score_diff;
    input(26, 0) = cycle_norm;

    m_dnn.Calculate(input);
    return m_dnn.mOutput(0, 0);
    } catch (const std::exception& e) {
        std::cerr << "[BoltShot] ML inference failed: " << e.what() << ", using heuristic" << std::endl;
        return heuristicScore(wm, shooter_pos, ball_pos, target_y);
    }
}

double BoltShotInference::heuristicScore(const WorldModel& wm,
                                         const Vector2D& shooter_pos,
                                         const Vector2D& ball_pos,
                                         double target_y) {
    static const double FIELD_LEN = 105.0;
    static const double FIELD_WID = 68.0;
    static const double GOAL_Y_TOP = 7.01;
    static const double GOAL_Y_BOT = -7.01;

    double target_x = (wm.ourSide() == LEFT) ? 52.5 : -52.5;
    double sx = shooter_pos.x, sy = shooter_pos.y;

    // Distance to target
    double dx = target_x - sx, dy = target_y - sy;
    double shot_dist = std::sqrt(dx * dx + dy * dy);

    // Shooting angle (goal opening)
    double top_ang = std::atan2(GOAL_Y_TOP - sy, target_x - sx);
    double bot_ang = std::atan2(GOAL_Y_BOT - sy, target_x - sx);
    double open_angle = std::abs(top_ang - bot_ang) * 180.0 / M_PI;

    // Count blockers on shot path
    int blockers = 0;
    double path_len = shot_dist;
    for (const AbstractPlayerObject* opp : wm.theirPlayers()) {
        if (!opp || !opp->pos().isValid()) continue;
        double ex = opp->pos().x - sx, ey = opp->pos().y - sy;
        double t = std::max(0.0, std::min(1.0, (ex * dx + ey * dy) / (path_len * path_len)));
        double px = sx + t * dx, py = sy + t * dy;
        double dist_to_path = std::sqrt((opp->pos().x - px) * (opp->pos().x - px) +
                                        (opp->pos().y - py) * (opp->pos().y - py));
        if (dist_to_path < 0.6) ++blockers;
    }

    // Heuristic score: favor close shots with wide angles and few blockers
    double score = 0.5; // baseline
    score += 0.3 * std::max(0.0, 1.0 - shot_dist / 30.0); // closer is better
    score += 0.2 * std::min(1.0, open_angle / 20.0); // wider angle is better
    score -= 0.15 * std::min(1.0, blockers / 3.0); // fewer blockers is better

    return std::max(0.0, std::min(1.0, score));
}

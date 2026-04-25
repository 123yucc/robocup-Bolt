#include "bolt_unmark_inference.h"

#include <rcsc/player/world_model.h>
#include <rcsc/player/abstract_player_object.h>
#include <rcsc/types.h>

#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <iostream>

using namespace rcsc;

DeepNueralNetwork BoltUnmarkInference::s_dnn;
bool              BoltUnmarkInference::s_loaded = false;

void BoltUnmarkInference::tryLoad(const std::string& weight_path) {
    if (!s_loaded) {
        s_loaded = s_dnn.ReadFromKeras(weight_path);
        if (s_loaded)
            std::cerr << "[BoltUnmark] loaded: " << weight_path << std::endl;
        else
            std::cerr << "[BoltUnmark] WARNING: failed to load " << weight_path << std::endl;
    }
}

bool BoltUnmarkInference::isLoaded() { return s_loaded; }

static double udist(double x1, double y1, double x2, double y2) {
    double dx = x1 - x2, dy = y1 - y2;
    return std::sqrt(dx * dx + dy * dy);
}

static double estimateOffsideX(const WorldModel& wm) {
    static const double GOAL_X = 52.5;
    std::vector<double> def_xs;
    for (const AbstractPlayerObject* opp : wm.theirPlayers()) {
        if (opp && opp->unum() > 0 && opp->pos().isValid())
            def_xs.push_back(opp->pos().x);
    }
    std::sort(def_xs.rbegin(), def_xs.rend());
    return (def_xs.size() >= 2) ? def_xs[1] : GOAL_X;
}

double BoltUnmarkInference::score(const WorldModel& wm,
                                  const Vector2D& candidate_pos,
                                  int passer_unum) {
    if (!s_loaded) return 0.0;

    static const double FIELD_LEN = 105.0;
    static const double FIELD_WID =  68.0;
    static const double HALF_LEN  = FIELD_LEN / 2.0;
    static const double HALF_WID  = FIELD_WID  / 2.0;
    static const double GOAL_X    = 52.5;

    // Passer
    const AbstractPlayerObject* passer = nullptr;
    if (passer_unum > 0) passer = wm.ourPlayer(passer_unum);
    if (!passer || !passer->pos().isValid()) {
        const AbstractPlayerObject* tm = wm.interceptTable().firstTeammate();
        if (tm && tm->unum() != wm.self().unum()) passer = tm;
    }

    double pa_x = 0.0, pa_y = 0.0, pa_vx = 0.0, pa_vy = 0.0;
    if (passer && passer->pos().isValid()) {
        pa_x = passer->pos().x; pa_y = passer->pos().y;
        pa_vx = passer->vel().x; pa_vy = passer->vel().y;
    }

    double ball_x = wm.ball().pos().x;
    double ball_y = wm.ball().pos().y;

    double rx    = candidate_pos.x;
    double ry    = candidate_pos.y;
    double r_vx  = wm.self().vel().x;
    double r_vy  = wm.self().vel().y;
    double r_stam = std::min(1.0, wm.self().stamina() / 8000.0);
    double r_body = wm.self().body().degree();

    double dist_to_passer   = udist(rx, ry, pa_x, pa_y);
    double att_goal_x       = (wm.ourSide() == LEFT) ? GOAL_X : -GOAL_X;
    double dist_to_goal     = udist(rx, ry, att_goal_x, 0.0);
    double dist_to_sideline = std::min(std::abs(ry - HALF_WID), std::abs(ry + HALF_WID));
    double dist_to_backline = std::abs(rx - att_goal_x);
    double dist_to_center   = std::abs(ry);

    double nearest_opp = 99.0, second_opp = 99.0;
    int    density_opp = 0;
    double passer_nearest_opp = 99.0;

    for (const AbstractPlayerObject* opp : wm.theirPlayers()) {
        if (!opp || !opp->pos().isValid() || opp->unum() < 1) continue;
        double d = udist(rx, ry, opp->pos().x, opp->pos().y);
        if (d < nearest_opp) { second_opp = nearest_opp; nearest_opp = d; }
        else if (d < second_opp) second_opp = d;
        if (d < 5.0) ++density_opp;
        double dp = udist(pa_x, pa_y, opp->pos().x, opp->pos().y);
        if (dp < passer_nearest_opp) passer_nearest_opp = dp;
    }

    int density_tm = 0;
    for (const AbstractPlayerObject* tm : wm.ourPlayers()) {
        if (!tm || !tm->pos().isValid() || tm->unum() == wm.self().unum()) continue;
        if (udist(rx, ry, tm->pos().x, tm->pos().y) < 5.0) ++density_tm;
    }

    double eta           = dist_to_passer / 2.0;
    double offside_x     = estimateOffsideX(wm);
    double offside_margin = (wm.ourSide() == LEFT) ? (offside_x - rx) : (rx - (-offside_x));
    double pass_angle    = std::atan2(ry - pa_y, rx - pa_x);

    int score_l = wm.gameMode().scoreLeft();
    int score_r = wm.gameMode().scoreRight();
    double score_diff = (wm.ourSide() == LEFT)
                        ? (score_l - score_r) / 5.0
                        : (score_r - score_l) / 5.0;
    double cycle_norm = static_cast<double>(wm.time().cycle()) / 6000.0;

    // Build 30-dim feature vector (matches ml/features/unmark_features.py)
    MatrixXd input(30, 1);
    input( 0, 0) = pa_x   / HALF_LEN;
    input( 1, 0) = pa_y   / HALF_WID;
    input( 2, 0) = pa_vx;
    input( 3, 0) = pa_vy;
    input( 4, 0) = 0.5;                          // passer stamina (not accessible via AbstractPlayerObject)
    input( 5, 0) = ball_x / HALF_LEN;
    input( 6, 0) = ball_y / HALF_WID;
    input( 7, 0) = rx     / HALF_LEN;
    input( 8, 0) = ry     / HALF_WID;
    input( 9, 0) = r_vx;
    input(10, 0) = r_vy;
    input(11, 0) = r_stam;
    input(12, 0) = dist_to_passer   / FIELD_LEN;
    input(13, 0) = dist_to_goal     / FIELD_LEN;
    input(14, 0) = dist_to_sideline / HALF_WID;
    input(15, 0) = dist_to_backline / FIELD_LEN;
    input(16, 0) = dist_to_center   / HALF_WID;
    input(17, 0) = nearest_opp      / FIELD_LEN;
    input(18, 0) = second_opp       / FIELD_LEN;
    input(19, 0) = eta              / 30.0;
    input(20, 0) = offside_margin   / FIELD_LEN;
    input(21, 0) = std::sin(pass_angle);
    input(22, 0) = std::cos(pass_angle);
    input(23, 0) = density_opp / 11.0;
    input(24, 0) = density_tm  / 10.0;
    input(25, 0) = passer_nearest_opp / FIELD_LEN;
    input(26, 0) = std::sin(r_body * M_PI / 180.0);
    input(27, 0) = std::cos(r_body * M_PI / 180.0);
    input(28, 0) = score_diff;
    input(29, 0) = cycle_norm;

    s_dnn.Calculate(input);
    return s_dnn.mOutput(0, 0);
}

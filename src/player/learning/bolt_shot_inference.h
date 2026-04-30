#pragma once

#include <CppDNN/DeepNueralNetwork.h>
#include <rcsc/geom/vector_2d.h>
#include <string>

namespace rcsc { class WorldModel; }

// ML-based shot target scorer.
// Mirrors ml/features/shot_target_features.py (27 features).
// Weight file must be in CppDNN ReadFromKeras() format.
// Score is in [0, 1]: probability that a shot toward target_y scores a goal.
class BoltShotInference {
public:
    static BoltShotInference& getInstance();

    void tryLoad(const std::string& weight_path);
    bool isLoaded() const;

    // shooter_pos : position of the player taking the shot
    // ball_pos    : current ball position
    // target_y    : y-coordinate of the goal target point (x = ±52.5)
    double score(const rcsc::WorldModel& wm,
                 const rcsc::Vector2D& shooter_pos,
                 const rcsc::Vector2D& ball_pos,
                 double target_y);

    // Heuristic fallback when ML model is not loaded
    double heuristicScore(const rcsc::WorldModel& wm,
                          const rcsc::Vector2D& shooter_pos,
                          const rcsc::Vector2D& ball_pos,
                          double target_y);

    // Delete copy constructor and assignment operator
    BoltShotInference(const BoltShotInference&) = delete;
    BoltShotInference& operator=(const BoltShotInference&) = delete;

private:
    BoltShotInference() : m_loaded(false) {}

    DeepNueralNetwork m_dnn;
    bool m_loaded;
};

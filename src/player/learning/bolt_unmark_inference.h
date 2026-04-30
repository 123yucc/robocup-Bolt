#pragma once

#include <CppDNN/DeepNueralNetwork.h>
#include <rcsc/geom/vector_2d.h>
#include <string>

namespace rcsc { class WorldModel; }

// ML-based unmark position scorer.
// Mirrors ml/features/unmark_features.py (30 features).
// Weight file must be in CppDNN ReadFromKeras() format.
// Score is in [0, 1]: probability that the candidate position is "receivable".
class BoltUnmarkInference {
public:
    static BoltUnmarkInference& getInstance();

    void tryLoad(const std::string& weight_path);
    bool isLoaded() const;

    // candidate_pos: the unmark target position being evaluated
    // passer_unum  : unum of the presumed passer (0 = use fastest teammate)
    double score(const rcsc::WorldModel& wm,
                 const rcsc::Vector2D& candidate_pos,
                 int passer_unum);

    // Heuristic fallback when ML model is not loaded
    double heuristicScore(const rcsc::WorldModel& wm,
                          const rcsc::Vector2D& candidate_pos,
                          int passer_unum);

    // Delete copy constructor and assignment operator
    BoltUnmarkInference(const BoltUnmarkInference&) = delete;
    BoltUnmarkInference& operator=(const BoltUnmarkInference&) = delete;

private:
    BoltUnmarkInference() : m_loaded(false) {}

    DeepNueralNetwork m_dnn;
    bool m_loaded;
};

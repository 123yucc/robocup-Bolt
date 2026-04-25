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
    static void tryLoad(const std::string& weight_path);
    static bool isLoaded();

    // candidate_pos: the unmark target position being evaluated
    // passer_unum  : unum of the presumed passer (0 = use fastest teammate)
    static double score(const rcsc::WorldModel& wm,
                        const rcsc::Vector2D& candidate_pos,
                        int passer_unum);

private:
    static DeepNueralNetwork s_dnn;
    static bool s_loaded;
};

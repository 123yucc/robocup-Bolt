#pragma once

#include <CppDNN/DeepNueralNetwork.h>
#include <rcsc/geom/vector_2d.h>
#include <string>

namespace rcsc {
    class WorldModel;
    class AbstractPlayerObject;
}

// ML-based pass decision scorer.
// Predicts the success probability of a pass from passer to receiver.
// Features: 35-dim (passer state, receiver state, ball state, pass params, opponent threat, tactical value)
// Weight file must be in CppDNN ReadFromKeras() format.
// Score is in [0, 1]: probability that the pass will succeed.
class BoltPassInference {
public:
    static void tryLoad(const std::string& weight_path);
    static bool isLoaded();

    // passer       : player attempting the pass
    // receiver     : target player to receive the pass
    // receiver_pos : target position where receiver will receive the ball
    // ball_pos     : current ball position
    // Returns: pass success probability [0, 1]
    static double score(const rcsc::WorldModel& wm,
                        const rcsc::Vector2D& passer_pos,
                        const rcsc::Vector2D& receiver_pos,
                        const rcsc::Vector2D& ball_pos);

private:
    static DeepNueralNetwork s_dnn;
    static bool s_loaded;
};

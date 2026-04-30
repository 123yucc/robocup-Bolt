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
    static BoltPassInference& getInstance();

    void tryLoad(const std::string& weight_path);
    bool isLoaded() const;

    // passer       : player attempting the pass
    // receiver     : target player to receive the pass
    // receiver_pos : target position where receiver will receive the ball
    // ball_pos     : current ball position
    // Returns: pass success probability [0, 1]
    double score(const rcsc::WorldModel& wm,
                 const rcsc::Vector2D& passer_pos,
                 const rcsc::Vector2D& receiver_pos,
                 const rcsc::Vector2D& ball_pos);

    // Heuristic fallback when ML model is not loaded
    double heuristicScore(const rcsc::WorldModel& wm,
                          const rcsc::Vector2D& passer_pos,
                          const rcsc::Vector2D& receiver_pos,
                          const rcsc::Vector2D& ball_pos);

    // Delete copy constructor and assignment operator
    BoltPassInference(const BoltPassInference&) = delete;
    BoltPassInference& operator=(const BoltPassInference&) = delete;

private:
    BoltPassInference() : m_loaded(false) {}

    DeepNueralNetwork m_dnn;
    bool m_loaded;
};

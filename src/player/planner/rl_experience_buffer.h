// -*-c++-*-
/*
 * Bolt Team - RoboCup 2D 2026
 * Reinforcement Learning Experience Replay Buffer
 *
 * Copyright: Wuhan University - Bolt Team
 * Implements circular buffer for DQN/PPO experience replay
 */

#ifndef RCSC_PLAYER_RL_EXPERIENCE_BUFFER_H
#define RCSC_PLAYER_RL_EXPERIENCE_BUFFER_H

#include <vector>
#include <deque>
#include <string>
#include <fstream>
#include <mutex>
#include <memory>

/*!
  \class RLExperienceBuffer
  \brief Stores (state, action, reward, next_state, terminal) tuples

  Supports:
  - Circular buffer with max size
  - Random sampling for training
  - CSV export for offline training
  - Thread-safe access
*/
class RLExperienceBuffer {
public:
    /*!
      \brief Single experience tuple
    */
    struct Experience {
        std::vector<double> state_features;      // State features (350-dim)
        int action_index;                        // Action taken (0-3: Pass/Shoot/Dribble/Hold)
        double reward;                           // Reward received
        std::vector<double> next_state_features; // Next state features
        bool is_terminal;                        // Episode ended?
        int cycle;                               // Game cycle
        int player_unum;                         // Player number

        Experience()
            : action_index(0)
            , reward(0.0)
            , is_terminal(false)
            , cycle(0)
            , player_unum(0)
        {}
    };

    /*!
      \brief Batch of experiences for training
    */
    struct ExperienceBatch {
        std::vector<std::vector<double>> states;
        std::vector<int> actions;
        std::vector<double> rewards;
        std::vector<std::vector<double>> next_states;
        std::vector<bool> terminals;

        size_t size() const { return states.size(); }
    };

    typedef std::shared_ptr<RLExperienceBuffer> Ptr;
    typedef std::shared_ptr<const RLExperienceBuffer> ConstPtr;

private:
    std::deque<Experience> M_buffer;      // Circular buffer
    int M_max_size;                       // Maximum buffer size
    std::mutex M_mutex;                   // Thread safety
    int M_state_dim;                      // Feature dimension
    bool M_enabled;                       // Buffer enabled

    // Statistics
    int M_total_added;
    double M_total_reward;
    int M_terminal_count;

public:
    /*!
      \brief Constructor
      \param max_size Maximum buffer size (default 50000)
      \param state_dim State feature dimension (default 350)
    */
    RLExperienceBuffer(int max_size = 50000, int state_dim = 350);

    /*!
      \brief Destructor
    */
    ~RLExperienceBuffer();

    /*!
      \brief Enable/disable buffer
    */
    void setEnabled(bool enabled) { M_enabled = enabled; }
    bool isEnabled() const { return M_enabled; }

    /*!
      \brief Add experience to buffer
      \param exp Experience tuple
    */
    void addExperience(const Experience & exp);

    /*!
      \brief Sample random batch for training
      \param batch_size Number of samples
      \return Batch of experiences
    */
    ExperienceBatch sampleBatch(int batch_size);

    /*!
      \brief Get recent experiences (for TD-learning)
      \param n Number of recent experiences
      \return Vector of experiences
    */
    std::vector<Experience> getRecentExperiences(int n);

    /*!
      \brief Clear buffer
    */
    void clear();

    /*!
      \brief Get current buffer size
      \return Number of experiences in buffer
    */
    size_t size() const;

    /*!
      \brief Check if buffer is empty
      \return true if empty
    */
    bool empty() const;

    /*!
      \brief Check if buffer has enough samples
      \param min_size Minimum required size
      \return true if buffer >= min_size
    */
    bool hasEnoughSamples(int min_size) const;

    /*!
      \brief Save buffer to CSV file
      \param filename Output file path
      \return true if successful
    */
    bool saveToCSV(const std::string & filename);

    /*!
      \brief Load buffer from CSV file
      \param filename Input file path
      \return true if successful
    */
    bool loadFromCSV(const std::string & filename);

    /*!
      \brief Get statistics
    */
    int getTotalAdded() const { return M_total_added; }
    double getAverageReward() const;
    int getTerminalCount() const { return M_terminal_count; }

    /*!
      \brief Get singleton instance
    */
    static RLExperienceBuffer & instance();
};

#endif // RCSC_PLAYER_RL_EXPERIENCE_BUFFER_H
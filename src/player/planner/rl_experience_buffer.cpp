// -*-c++-*-
/*
 * Bolt Team - RoboCup 2D 2026
 * Reinforcement Learning Experience Replay Buffer Implementation
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "rl_experience_buffer.h"

#include <rcsc/common/logger.h>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>

using namespace rcsc;

// Global singleton instance
namespace {
    RLExperienceBuffer * g_rl_experience_buffer_instance = nullptr;
}

/*-------------------------------------------------------------------*/
RLExperienceBuffer::RLExperienceBuffer(int max_size, int state_dim)
    : M_max_size(max_size)
    , M_state_dim(state_dim)
    , M_enabled(true)
    , M_total_added(0)
    , M_total_reward(0.0)
    , M_terminal_count(0)
{
    dlog.addText(Logger::TEAM,
                 __FILE__": (RLExperienceBuffer) Created buffer with max_size=%d, state_dim=%d",
                 max_size, state_dim);
}

/*-------------------------------------------------------------------*/
RLExperienceBuffer::~RLExperienceBuffer()
{
    dlog.addText(Logger::TEAM,
                 __FILE__": (RLExperienceBuffer) Destroyed. Total added=%d, avg_reward=%.2f",
                 M_total_added, getAverageReward());
}

/*-------------------------------------------------------------------*/
RLExperienceBuffer &
RLExperienceBuffer::instance()
{
    if (!g_rl_experience_buffer_instance)
    {
        g_rl_experience_buffer_instance = new RLExperienceBuffer(50000, 350);
    }
    return *g_rl_experience_buffer_instance;
}

/*-------------------------------------------------------------------*/
void
RLExperienceBuffer::addExperience(const Experience & exp)
{
    if (!M_enabled)
        return;

    std::lock_guard<std::mutex> lock(M_mutex);

    // Check buffer size and remove oldest if full
    if (M_buffer.size() >= M_max_size)
    {
        // Remove oldest experience
        const Experience & oldest = M_buffer.front();
        M_total_reward -= oldest.reward;
        if (oldest.is_terminal)
            M_terminal_count--;
        M_buffer.pop_front();
    }

    // Add new experience
    M_buffer.push_back(exp);
    M_total_added++;
    M_total_reward += exp.reward;

    if (exp.is_terminal)
        M_terminal_count++;

    // Log periodically
    if (M_total_added % 1000 == 0)
    {
        dlog.addText(Logger::TEAM,
                     __FILE__": (addExperience) buffer_size=%zu, total_added=%d, avg_reward=%.2f",
                     M_buffer.size(), M_total_added, getAverageReward());
    }
}

/*-------------------------------------------------------------------*/
RLExperienceBuffer::ExperienceBatch
RLExperienceBuffer::sampleBatch(int batch_size)
{
    std::lock_guard<std::mutex> lock(M_mutex);

    ExperienceBatch batch;

    if (M_buffer.size() < batch_size)
    {
        // Return empty batch if not enough samples
        return batch;
    }

    // Random sampling
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, M_buffer.size() - 1);

    for (int i = 0; i < batch_size; ++i)
    {
        size_t idx = dist(gen);
        const Experience & exp = M_buffer[idx];

        batch.states.push_back(exp.state_features);
        batch.actions.push_back(exp.action_index);
        batch.rewards.push_back(exp.reward);
        batch.next_states.push_back(exp.next_state_features);
        batch.terminals.push_back(exp.is_terminal);
    }

    return batch;
}

/*-------------------------------------------------------------------*/
std::vector<RLExperienceBuffer::Experience>
RLExperienceBuffer::getRecentExperiences(int n)
{
    std::lock_guard<std::mutex> lock(M_mutex);

    std::vector<Experience> recent;

    int count = std::min(n, static_cast<int>(M_buffer.size()));

    for (int i = 0; i < count; ++i)
    {
        recent.push_back(M_buffer[M_buffer.size() - 1 - i]);
    }

    return recent;
}

/*-------------------------------------------------------------------*/
void
RLExperienceBuffer::clear()
{
    std::lock_guard<std::mutex> lock(M_mutex);

    M_buffer.clear();
    M_total_reward = 0.0;
    M_terminal_count = 0;
    // Keep M_total_added for statistics

    dlog.addText(Logger::TEAM,
                 __FILE__": (clear) Buffer cleared");
}

/*-------------------------------------------------------------------*/
size_t
RLExperienceBuffer::size() const
{
    return M_buffer.size();
}

/*-------------------------------------------------------------------*/
bool
RLExperienceBuffer::empty() const
{
    return M_buffer.empty();
}

/*-------------------------------------------------------------------*/
bool
RLExperienceBuffer::hasEnoughSamples(int min_size) const
{
    return M_buffer.size() >= min_size;
}

/*-------------------------------------------------------------------*/
double
RLExperienceBuffer::getAverageReward() const
{
    if (M_buffer.empty())
        return 0.0;
    return M_total_reward / M_buffer.size();
}

/*-------------------------------------------------------------------*/
bool
RLExperienceBuffer::saveToCSV(const std::string & filename)
{
    std::lock_guard<std::mutex> lock(M_mutex);

    std::ofstream fout(filename);

    if (!fout.is_open())
    {
        dlog.addText(Logger::TEAM,
                     __FILE__": (saveToCSV) Failed to open file: %s",
                     filename.c_str());
        return false;
    }

    // Write header
    fout << "# RL Experience Buffer Export - Bolt Team RoboCup 2D 2026\n";
    fout << "# Total experiences: " << M_buffer.size() << "\n";
    fout << "# State dimension: " << M_state_dim << "\n";
    fout << "# Format: state_features,action,reward,next_state_features,is_terminal,cycle,player_unum\n";

    // Write each experience
    for (const Experience & exp : M_buffer)
    {
        // State features
        for (size_t i = 0; i < exp.state_features.size(); ++i)
        {
            fout << exp.state_features[i];
            if (i < exp.state_features.size() - 1)
                fout << ",";
        }
        fout << ";";

        // Action
        fout << exp.action_index << ";";

        // Reward
        fout << std::fixed << std::setprecision(4) << exp.reward << ";";

        // Next state features
        for (size_t i = 0; i < exp.next_state_features.size(); ++i)
        {
            fout << exp.next_state_features[i];
            if (i < exp.next_state_features.size() - 1)
                fout << ",";
        }
        fout << ";";

        // Terminal flag
        fout << (exp.is_terminal ? 1 : 0) << ";";

        // Cycle and player
        fout << exp.cycle << ";" << exp.player_unum << "\n";
    }

    fout.close();

    dlog.addText(Logger::TEAM,
                 __FILE__": (saveToCSV) Saved %zu experiences to %s",
                 M_buffer.size(), filename.c_str());

    return true;
}

/*-------------------------------------------------------------------*/
bool
RLExperienceBuffer::loadFromCSV(const std::string & filename)
{
    std::lock_guard<std::mutex> lock(M_mutex);

    std::ifstream fin(filename);

    if (!fin.is_open())
    {
        dlog.addText(Logger::TEAM,
                     __FILE__": (loadFromCSV) Failed to open file: %s",
                     filename.c_str());
        return false;
    }

    std::string line;
    int loaded_count = 0;

    while (std::getline(fin, line))
    {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        Experience exp;
        std::istringstream iss(line);
        std::string segment;

        // Parse state features
        if (std::getline(iss, segment, ';'))
        {
            std::istringstream state_ss(segment);
            std::string val;
            while (std::getline(state_ss, val, ','))
            {
                exp.state_features.push_back(std::stod(val));
            }
        }

        // Parse action
        if (std::getline(iss, segment, ';'))
        {
            exp.action_index = std::stoi(segment);
        }

        // Parse reward
        if (std::getline(iss, segment, ';'))
        {
            exp.reward = std::stod(segment);
        }

        // Parse next state features
        if (std::getline(iss, segment, ';'))
        {
            std::istringstream next_ss(segment);
            std::string val;
            while (std::getline(next_ss, val, ','))
            {
                exp.next_state_features.push_back(std::stod(val));
            }
        }

        // Parse terminal flag
        if (std::getline(iss, segment, ';'))
        {
            exp.is_terminal = (std::stoi(segment) == 1);
        }

        // Parse cycle
        if (std::getline(iss, segment, ';'))
        {
            exp.cycle = std::stoi(segment);
        }

        // Parse player unum
        if (std::getline(iss, segment, ';'))
        {
            exp.player_unum = std::stoi(segment);
        }

        // Add to buffer
        if (M_buffer.size() >= M_max_size)
        {
            M_buffer.pop_front();
        }
        M_buffer.push_back(exp);
        loaded_count++;
    }

    fin.close();

    dlog.addText(Logger::TEAM,
                 __FILE__": (loadFromCSV) Loaded %d experiences from %s",
                 loaded_count, filename.c_str());

    return true;
}
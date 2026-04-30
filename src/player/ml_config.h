#pragma once

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>

// ML模型配置管理类
// 从ml_config.conf读取ML模型权重文件路径
class MLConfig {
public:
    static MLConfig& getInstance() {
        static MLConfig instance;
        return instance;
    }

    // 加载配置文件
    bool load(const std::string& config_file = "./ml_config.conf") {
        std::ifstream ifs(config_file);
        if (!ifs.is_open()) {
            std::cerr << "[MLConfig] WARNING: Cannot open " << config_file
                      << ", using default paths" << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(ifs, line)) {
            // 跳过注释和空行
            if (line.empty() || line[0] == '#') continue;

            // 解析 key : value 格式
            size_t pos = line.find(':');
            if (pos == std::string::npos) continue;

            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));

            m_config[key] = value;
        }

        std::cerr << "[MLConfig] Loaded configuration from " << config_file << std::endl;
        return true;
    }

    // 获取配置值
    std::string get(const std::string& key, const std::string& default_value = "") const {
        auto it = m_config.find(key);
        if (it != m_config.end()) {
            return it->second;
        }
        return default_value;
    }

    // 获取ML权重文件完整路径
    std::string getWeightPath(const std::string& model_name) const {
        std::string dir = get("ml_weights_dir", "./");
        std::string filename = get(model_name, "");

        if (filename.empty()) {
            std::cerr << "[MLConfig] WARNING: No config for " << model_name << std::endl;
            return "";
        }

        // 确保目录以/结尾
        if (!dir.empty() && dir.back() != '/') {
            dir += '/';
        }

        return dir + filename;
    }

    // 删除拷贝构造和赋值
    MLConfig(const MLConfig&) = delete;
    MLConfig& operator=(const MLConfig&) = delete;

private:
    MLConfig() {
        // 设置默认值
        m_config["ml_weights_dir"] = "./";
        m_config["ml_shot_model"] = "shot_target_mlp_weights.txt";
        m_config["ml_unmark_model"] = "unmark_mlp_weights.txt";
        m_config["ml_pass_model"] = "pass_decision_mlp_weights.txt";
        m_config["ml_defense_model"] = "defense_positioning_mlp_weights.txt";
    }

    // 去除字符串首尾空白
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }

    std::map<std::string, std::string> m_config;
};

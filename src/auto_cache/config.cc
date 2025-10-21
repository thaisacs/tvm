
#include "config.h"

using namespace tvm;
using namespace tvm::auto_cache;

extern "C" int nw_cmdline(const char*, const char*);

void Trim(std::string& s) {
    s.erase(std::remove(s.begin(), s.end(), '\t'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '\"'), s.end());
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    s.erase(std::remove(s.begin(), s.end(), ','), s.end());
    s.erase(std::remove(s.begin(), s.end(), '['), s.end());
    s.erase(std::remove(s.begin(), s.end(), ']'), s.end());
}

Config::Config(std::string filename) {
    std::ifstream file(filename);
    std::string line;
    std::string currentKey;

    while (std::getline(file, line)) {
        // Remove leading/trailing whitespace
        std::string trimmed = line;
        Trim(trimmed);

        if (trimmed.empty() || trimmed == "{" || trimmed == "}") continue;

        if (trimmed.back() == ':') {
            // Key line
            currentKey = trimmed.substr(0, trimmed.size() - 1);
            Trim(currentKey);
        } else if (!currentKey.empty()) {
            // Value line
            Trim(trimmed);
            this->configs[currentKey].push_back(trimmed);
        }
    }
}

std::vector<std::string> Config::GetCacheFiles(std::string mod_dna) {
    int similarity_value, best_similarity = -100000;
    std::vector<std::string> results;
    for (const auto& kv : this->configs) {
        std::string dna = kv.first;
        similarity_value = nw_cmdline(mod_dna.c_str(), dna.c_str());
        if(similarity_value > best_similarity) {
            best_similarity = similarity_value;
            results = kv.second;
        }
    }
    std::cout << "==============================\n";
    std::cout << "Best similarity: " << best_similarity << std::endl;
    std::cout << mod_dna.size() << " vs " << best_similarity << std::endl;
    std::cout << "==============================\n";
    return results;
}

//std::vector<std::string> Config::GetCacheFiles(std::string mod_dna) {
//    int similarity_value;
//    float similarity_value_f;
//    std::vector<std::string> results;
//    for (const auto& kv : this->configs) {
//        std::string dna = kv.first;
//        similarity_value = nw_cmdline(mod_dna.c_str(), dna.c_str());
//        if(similarity_value > 0) {
//            similarity_value_f = static_cast<float>(similarity_value) / mod_dna.size();
//            std::cout << mod_dna.size() << " , " << dna.size() << " , " << similarity_value << std::endl;
//            std::cout << "Similarity with " << dna << " : " << similarity_value_f << std::endl; 
//            if(similarity_value_f >= 0.3f) {
//                for(const std::string& file : kv.second) {
//                    results.push_back(file);
//                }
//
//            }
//        }
//    }
//    std::cout << "==============================\n";
//    std::cout << results.size() << " similar cache files found." << std::endl;
//    std::cout << "==============================\n";
//    return results;
//}

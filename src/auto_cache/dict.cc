#include "dict.h"

using namespace tvm;
using namespace tvm::auto_cache;

Dict::Dict() {}

Dict::Dict(std::string filename) {
    this->LoadFromFile(filename);
}

void Dict::LoadFromFile(std::string filename) {
    std::ifstream file(filename);
    std::string line;

    std::string key;
    std::string value;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        if (std::getline(ss, key, ',') && std::getline(ss, value)) {
            if(value == "Value\r") {
                continue;
            }
            std::string clean;
            for (char c : value) {
                if (std::isprint(static_cast<unsigned char>(c)) && c != '\r') {
                    clean += c;
                }
            }
            this->dict[key] = clean;
        }
    }

}

std::string Dict::FindToken(const std::string& token) {
    auto it = dict.find(token);
    return (it != dict.end()) ? it->second : "NA";
}

bool Dict::Has(std::string token) {
    return true;
    if (this->dict.find(token) != this->dict.end()) {
        return true;
    } else {
        return false;
    }
}

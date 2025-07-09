#include "util.h"

#include "../meta_schedule/utils.h"

namespace tvm {
namespace auto_cache {

std::vector<std::string> get_name_parts(std::string input) {
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = input.find('_');

    while (end != std::string::npos) {
        parts.push_back(input.substr(start, end - start));
        start = end + 1;
        end = input.find('_', start);
    }
    parts.push_back(input.substr(start));

    return parts;
}

std::string get_hash(std::string task_name) {
    std::string hash = "";
    std::vector parts = get_name_parts(task_name);
    for(const auto& part: parts) {
        int end = part.size() - 1;
        for(int i = part.size() - 1; i >= 0; i--) {
            if (part[i] == '1' ||
                part[i] == '2' ||
                part[i] == '3' ||
                part[i] == '4' ||
                part[i] == '5' ||
                part[i] == '6' ||
                part[i] == '7' ||
                part[i] == '8' ||
                part[i] == '9' ||
                part[i] == '0'
            ) {
                end = i - 1;
            }else {
                break;
            }
        }
        if(hash.size() > 0) {
            hash += '-';
        }
        hash += part.substr(0, end+1);
    }
    return hash;
}

Config read_cache_file(std::string cache_file) {
    Config data;
    auto InputPBuffer = llvm::MemoryBuffer::getFile(cache_file);
    llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
    yinp >> data;
    return data;
}

TaskData read_log_file(std::string log_file) {
    TaskData data;
    auto InputPBuffer = llvm::MemoryBuffer::getFile(log_file);
    llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
    yinp >> data;
    return data;
}

std::string get_transformations(std::string state_str) {
    std::string state_new;
    for(long unsigned int i = 0; i < state_str.size(); i++) {
        if(state_str[i] == '\'') {
            state_new.push_back('"');
        }else if(state_str[i] == '"') {
            state_new.push_back('\\');
            state_new.push_back('"');
        }else if(state_str[i] == '\n') {
            i = i + 1;
        }else {
            state_new.push_back(state_str[i]);
        }
    }
    return state_new;
}

Params read_params_file(std::string params_file) {
    Params params;
    auto InputPBuffer = llvm::MemoryBuffer::getFile(params_file);
    llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
    yinp >> params;
    return params;
}

}
}

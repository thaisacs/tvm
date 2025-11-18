#include "util.h"

#include "../../../meta_schedule/utils.h"

namespace tvm {
namespace auto_cache {

std::vector<std::string> Split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end;

    while ((end = str.find(delimiter, start)) != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
    }

    tokens.push_back(str.substr(start));  // last token
    return tokens;
}

std::vector<std::string> Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }

    return tokens;
}

std::vector<std::string> GetNameParts(std::string input) {
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

std::string GetHash(std::string task_name) {
    std::string hash = "";
    std::vector parts = GetNameParts(task_name);
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

TaskData ReadLogFile(std::string log_file) {
    TaskData data;
    auto InputPBuffer = llvm::MemoryBuffer::getFile(log_file);
    llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
    yinp >> data;
    return data;
}

std::string GetTransformations(std::string state_str) {
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

Params ReadParamsFile(std::string params_file) {
    Params params;
    auto InputPBuffer = llvm::MemoryBuffer::getFile(params_file);
    llvm::yaml::Input yinp(InputPBuffer->get()->getBuffer());
    yinp >> params;
    return params;
}

std::vector<unsigned int> GenModShapes(std::string mod_str) {
    std::vector<unsigned int> mod_shapes;
    std::vector<std::string> lines = Split(mod_str, '\n');
    for(int i = 0; i < static_cast<int>(lines.size()); i++) {
        std::string line = lines[i];
        std::string main_sub = "main";
        std::string block_sub = "T.block(";
        std::string root = "root";
        if (line.find(main_sub) != std::string::npos) {
            std::vector<std::string> values = Split(line, ' ');
            std::string int_sub = "T.int64";
            for(int j = 0; j < static_cast<int>(values.size()); j++) {
                std::string value = values[j];
                if(value.find(int_sub) != std::string::npos) {
                    std::vector<std::string> value_parts = Split(value, int_sub);
                    std::string shape = value_parts[1];
                    shape.erase(std::remove(shape.begin(), shape.end(), '('), shape.end());
                    shape.erase(std::remove(shape.begin(), shape.end(), ')'), shape.end());
                    shape.erase(std::remove(shape.begin(), shape.end(), ','), shape.end());
                    mod_shapes.push_back(static_cast<unsigned int>(std::stoul(shape)));
                }
            }
        }
    }
    return mod_shapes;
}

std::vector<unsigned int> GenRecordShapes(std::string record_str) {
    std::vector<unsigned int> record_shapes;
    std::vector<std::string> lines = Split(record_str, '\n');
    for(int i = 0; i < static_cast<int>(lines.size()); i++) {
        std::string line = lines[i];
        std::vector<std::string> values = Split(line, ' ');
        std::string tensor_sub = "TENSOR";
        for(int j = 0; j < static_cast<int>(values.size()); j++) {
            std::string value = values[j];
            if (value.find(tensor_sub) != std::string::npos) {
                std::string shape = values[j+2];
                shape.erase(std::remove(shape.begin(), shape.end(), '['), shape.end());
                shape.erase(std::remove(shape.begin(), shape.end(), ','), shape.end());
                record_shapes.push_back(static_cast<unsigned int>(std::stoul(shape)));
                shape = values[j+3];
                shape.erase(std::remove(shape.begin(), shape.end(), ','), shape.end());
                record_shapes.push_back(static_cast<unsigned int>(std::stoul(shape)));
                shape = values[j+4];
                shape.erase(std::remove(shape.begin(), shape.end(), ','), shape.end());
                record_shapes.push_back(static_cast<unsigned int>(std::stoul(shape)));
                shape = values[j+5];
                shape.erase(std::remove(shape.begin(), shape.end(), ']'), shape.end());
                shape.erase(std::remove(shape.begin(), shape.end(), ','), shape.end());
                record_shapes.push_back(static_cast<unsigned int>(std::stoul(shape)));
            }
        }
    }
    return record_shapes;
}

std::vector<std::vector<unsigned int>> GenParams(std::string record_str) {
    bool new_vector = false;
    std::string shape;
    std::vector<unsigned int> buffer;
    std::vector<std::vector<unsigned int>> params;
    std::vector<std::string> lines = Split(record_str, '\n');
    for(int i = 0; i < static_cast<int>(lines.size()); i++) {
        std::string line = lines[i];
        std::vector<std::string> values = Split(line, "]]], [[");
        values = Split(values[1], "]]], [");
        values = Split(values[0], ", ");
        for(int j = 0; j < static_cast<int>(values.size()); j++) {
            std::string value = values[j];
            if(values.size() - 1 > j && values[j+1][0] == '[' && values[j][values[j].size()-1] != ']') {
                new_vector = true;
            }else {
                shape = values[j];
                shape.erase(std::remove(shape.begin(), shape.end(), '['), shape.end());
                shape.erase(std::remove(shape.begin(), shape.end(), ']'), shape.end());
                buffer.push_back(static_cast<unsigned int>(std::stoul(shape)));
                if(new_vector && values[j][values[j].size()-1] == ']') {
                    new_vector = false;
                    params.push_back(buffer);
                    buffer.clear();
                }
            }
        }
    }
    return params;
}

unsigned int multiply_vector(const std::vector<unsigned int>& vec) {
    unsigned int result = 1;
    for(long unsigned int i = 0; i < vec.size(); i++) {
        result = result * vec[i];
    }
    return result;
}

std::vector<unsigned int> FixShapes(std::vector<unsigned int> param, unsigned int target_shape) {
    std::vector<unsigned int> new_param;
    new_param.push_back(target_shape);
    for(long unsigned int i = 1; i < param.size(); i++) {
        new_param.push_back(1);
    }
    return new_param;
}

void ReplaceSubstring(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Move past the replaced part
    }
}

std::string GenNewRecordString(std::string record_string, std::vector<std::vector<unsigned int>> params) {
    bool new_vector = false;
    std::string new_sub_string;
    std::string sub_string;
    unsigned int param_index = 0;
    std::string shape, value;
    std::vector<std::string> lines = Split(record_string, '\n');
    for(int i = 0; i < static_cast<int>(lines.size()); i++) {
        std::string line = lines[i];
        std::vector<std::string> values = Split(line, "]]], [[");
        values = Split(values[1], "]]], [");
        std::cout << values[0] << std::endl;
        sub_string = values[0];
        values = Split(values[0], ", ");
        for(int j = 0; j < static_cast<int>(values.size()); j++) {
            value = values[j];
            if(values.size() - 1 > j && values[j+1][0] == '[' && values[j][values[j].size()-1] != ']') {
                shape = values[j];
                shape.erase(std::remove(shape.begin(), shape.end(), '['), shape.end());
                shape.erase(std::remove(shape.begin(), shape.end(), ','), shape.end());
                if(param_index > 0) {
                    new_sub_string += ", [";
                }
                new_sub_string += shape + ", [";
                for(long unsigned int k = 0; k < params[param_index].size(); k++) {
                    new_sub_string += std::to_string(params[param_index][k]);
                    if(k != params[param_index].size() - 1) {
                        new_sub_string += ", ";
                    }
                }
                new_sub_string += "]]";
                new_vector = true;
                param_index = param_index + 1;
            }else {
                shape = values[j];
                if(!new_vector && param_index == params.size()) {
                    std::cout << new_vector << " " << shape << std::endl;
                    new_sub_string += ", " + shape;

                }
                if(new_vector && values[j][values[j].size()-1] == ']') {
                    new_vector = false;
                }
            }
        }
        std::cout << new_sub_string << std::endl;
        ReplaceSubstring(record_string, sub_string, new_sub_string);
        std::cout << record_string << std::endl;
    }
    return record_string; 
}

std::string FixParams(std::string task_name, std::string mod_str, std::string record_string) {
    bool modified = false;
    std::vector<unsigned int> mod_shapes;
    std::vector<unsigned int> record_shapes;
    std::vector<std::vector<unsigned int>> params;
    std::string hash = GetHash(task_name);
    if(hash == "fused-conv2d-add-relu") {
        std::cout << "Task name: " << task_name << ", Hash: " << hash << std::endl;

        mod_shapes = GenModShapes(mod_str);
        record_shapes = GenRecordShapes(record_string);
        params = GenParams(record_string);

        //for(long unsigned int i = 0; i < params.size(); i++) {
        //    std::cout << "Param " << i << ": ";
        //    for(long unsigned int j = 0; j < params[i].size(); j++) {
        //        std::cout << params[i][j] << " ";
        //    }
        //    std::cout << std::endl;
        //}

        if(multiply_vector(params[1]) != mod_shapes[4]) {
            params[1] = FixShapes(params[1], mod_shapes[4]);
            modified = true;
        }

        for(long unsigned int i = 2; i < params.size(); i++) {
            unsigned int mult = multiply_vector(params[i]);

            unsigned int k = 0;
            for(; k < record_shapes.size(); k++) {
                if(record_shapes[k] == mult) {
                    break;
                }
            }
            unsigned int current_mult = multiply_vector(params[i]);
            unsigned int target_shape = mod_shapes[k];
            if(current_mult != target_shape) {
                std::cout << "Current mult: " << current_mult << ", Target shape: " << target_shape << std::endl;
                params[i] = FixShapes(params[i], mod_shapes[k]);
                modified = true;
            }
        }

        //for(long unsigned int i = 0; i < params.size(); i++) {
        //    std::cout << "Param " << i << ": ";
        //    for(long unsigned int j = 0; j < params[i].size(); j++) {
        //        std::cout << params[i][j] << " ";
        //    }
        //    std::cout << std::endl;
        //}

        if(!modified) {
            return record_string;
        }else {
            return GenNewRecordString(record_string, params);
        }
    }
    return record_string;
}

}
}

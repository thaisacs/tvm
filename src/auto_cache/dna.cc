#include "dna.h"
#include "util.h"

using namespace tvm;
using namespace tvm::auto_cache;

std::vector<std::string> DNA::Split(const std::string& str, const std::string& delimiter) {
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

std::vector<std::string> DNA::Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }

    return tokens;
}

std::string DNA::RenameBlock(std::string block_name) {
    std::string rename = "";
    std::vector<std::string> parts = Split(block_name, '_');
    for(int i = 0; i < static_cast<int>(parts.size()); i++) {
        std::string part = parts[i];
        if(i > 1) {
            bool has_digit = false;
            for(char c : part) {
                if (isdigit(c)) {
                    has_digit = true;
                    break;
                }
            }
            if(has_digit) {
                continue;
            }
        }
        if(rename.size() > 0) {
            rename = rename + '_' + part;
        }else {
            rename = part;
        }
    }
    return rename;
}

std::string DNA::DNAGenerator(std::vector<std::string> program_tokens, std::unique_ptr<Dict> dict) {
    std::vector<std::string> results;
    std::string buffer;
    for(long unsigned int i = 0; i < program_tokens.size(); i++) {
        std::string token = program_tokens[i];
        if(dict->Has(token)) {
            buffer = dict->FindToken(token);
        }else {
            buffer = "-";
        }
        if(i == program_tokens.size() - 1) {
            for(long unsigned int j = 0; j < program_tokens.size(); j++) {
                results.push_back(buffer);
            }
        }else {
            results.push_back(buffer);
        }
    }
    std::string dna = "";
    for(int i = 0; i < static_cast<int>(results.size()); i++) {
        buffer = results[i];
        dna += buffer;
    }
    return dna;
}

std::string DNA::ConvertMod2DNA(std::string mod_str, std::unique_ptr<Dict> dict, std::string task_name) {
    std::vector<std::string> program_tokens;
    std::vector<std::string> lines = this->Split(mod_str, '\n');
    for(int i = 0; i < static_cast<int>(lines.size()); i++) {
        std::string line = lines[i];
        std::string main_sub = "main";
        std::string block_sub = "T.block(";
        std::string root = "root";
        if (line.find(main_sub) != std::string::npos) {
            std::vector<std::string> values = this->Split(line, ' ');
            std::string int_sub = "T.int64";
            for(int j = 0; j < static_cast<int>(values.size()); j++) {
                std::string value = values[j];
                if(value.find(int_sub) != std::string::npos) {
                    std::vector<std::string> value_parts = this->Split(value, int_sub);
                    std::string shape = value_parts[1];
                    shape.erase(std::remove(shape.begin(), shape.end(), '('), shape.end());
                    shape.erase(std::remove(shape.begin(), shape.end(), ')'), shape.end());
                    shape.erase(std::remove(shape.begin(), shape.end(), ','), shape.end());
                    program_tokens.push_back(shape);
                }
            }
        }
        //if (line.find(block_sub) != std::string::npos and line.find(root) == std::string::npos) {
        //    std::vector<std::string> x = this->Split(line, ' ');
        //    std::string block_name = this->Split(x[x.size()-1], '(')[1];
        //    block_name = this->Split(block_name, ')')[0];
        //    block_name.erase(std::remove(block_name.begin(), block_name.end(), '\"'), block_name.end());
        //    if(block_name.find("lv") != std::string::npos) {
        //        block_name = "mean";
        //    }
        //    block_name = this->RenameBlock(block_name);
        //    program_tokens.push_back(this->RenameBlock(block_name));
        //}
    }
    std::string op = GetHash(task_name);
    program_tokens.push_back(op);
    return this->DNAGenerator(program_tokens, std::move(dict));
}

DNA::DNA(std::string mod_str, std::unique_ptr<Dict> dict, std::string task_name) {
    this->gene = this->ConvertMod2DNA(mod_str, std::move(dict), task_name);
}

std::string DNA::DumpGene() {
    return this->gene;
}

//Dict::Dict(std::string filename) {
//    this->LoadFromFile(filename);
//}
//
//void Dict::LoadFromFile(std::string filename) {
//    std::ifstream file(filename);
//    std::string line;
//
//    while (std::getline(file, line)) {
//        std::istringstream ss(line);
//        std::string key;
//        std::string value;
//        if (std::getline(ss, key, ',') && std::getline(ss, value)) {
//            this->dict[key] = value;
//        }
//    }
//
//}

//void Dict::Add(std::string key, std::string value) {
//    dict[key] = value;
//    //for (const auto& [key, value] : dict) {
//    //    std::cout << key << " => " << value << std::endl;
//    //}
//}
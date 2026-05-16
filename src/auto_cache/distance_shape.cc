#include "distance_shape.h"
#include "util.h"

using namespace tvm;
using namespace tvm::auto_cache;

std::vector<std::string> DistanceShape::Split(const std::string& str, const std::string& delimiter) {
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

std::vector<std::string> DistanceShape::Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        tokens.push_back(item);
    }

    return tokens;
}

std::string DistanceShape::RenameBlock(std::string block_name) {
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

void DistanceShape::setShapes(std::string mod_str) {
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
                    int num = std::stoi(shape);
                    this->shapes.push_back(num);
                }
            }
        }
    }
}

void DistanceShape::dumpShapes(std::vector<int> shapes) {
    for(int i = 0; i < shapes.size(); i++) {
        std::cout << shapes[i] << " ";
    }
    std::cout << std::endl;
}

int DistanceShape::ComputeDistance(std::string sequence_shapes) {
    std::vector<std::string> values = this->Split(sequence_shapes, ' ');
    std::vector<int> shapes;
    for(int j = 0; j < static_cast<int>(values.size()); j++) {
        std::string shape = values[j];
        shape.erase(std::remove(shape.begin(), shape.end(), '['), shape.end());
        shape.erase(std::remove(shape.begin(), shape.end(), ']'), shape.end());
        shape.erase(std::remove(shape.begin(), shape.end(), ','), shape.end());
        int num = std::stoi(shape);
        shapes.push_back(num);
    }
    int buffer = 0, sum = 0;
    for(int i = 0; i < this->shapes.size(); i++) {
        buffer = this->shapes[i] - shapes[i];
        if(buffer < 0) {
            buffer = (-1) * buffer;
        }
        sum += buffer;
    }
    return sum;
}

DistanceShape::DistanceShape(std::string mod_str) {
    this->setShapes(mod_str);
}

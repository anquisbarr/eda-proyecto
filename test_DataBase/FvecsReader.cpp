#include "FvecsReader.hpp"
#include <fstream>
#include <iostream>

SIFTNode::SIFTNode(const std::vector<float>& vec) : descriptor(vec) {}

std::vector<SIFTNode> FvecsReader::read(const std::string& filepath) {
    std::vector<SIFTNode> nodes;
    std::ifstream file(filepath, std::ios::binary);

    if (!file) {
        std::cerr << "Error abriendo el archivo: " << filepath << std::endl;
        return nodes;
    }

    while (file.peek() != EOF) {
        int dim;
        file.read(reinterpret_cast<char*>(&dim), sizeof(int));
        if (file.eof()) break;

        std::vector<float> vec(dim);
        file.read(reinterpret_cast<char*>(vec.data()), dim * sizeof(float));
        if (file.gcount() < static_cast<std::streamsize>(dim * sizeof(float))) break;

        nodes.emplace_back(vec);
    }

    file.close();
    return nodes;
}
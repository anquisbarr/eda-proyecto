#ifndef FVECS_READER_HPP
#define FVECS_READER_HPP

#include <vector>
#include <string>

struct SIFTNode {
    std::vector<float> descriptor;
    
    SIFTNode(const std::vector<float>& vec);
};

class FvecsReader {
public:
    static std::vector<SIFTNode> read(const std::string& filepath);
};

#endif
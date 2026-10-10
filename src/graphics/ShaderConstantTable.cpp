#include "graphics/ShaderConstantTable.h"

#include <cstring>

namespace hca::graphics {

namespace {

constexpr std::uint32_t kCommentToken = 0xFFFE;
constexpr std::uint32_t kEndToken = 0x0000FFFF;
constexpr std::uint32_t kCtabFourCc = 0x42415443;  // 'CTAB' en petit-boutiste

// Disposition de D3DXSHADER_CONSTANTTABLE et D3DXSHADER_CONSTANTINFO (d3dx9shader.h), recopiée pour ne pas
// dépendre de D3DX.
struct CtabHeader {
    std::uint32_t size;
    std::uint32_t creator;
    std::uint32_t version;
    std::uint32_t constants;
    std::uint32_t constantInfo;
    std::uint32_t flags;
    std::uint32_t target;
};

struct CtabConstantInfo {
    std::uint32_t name;
    std::uint16_t registerSet;
    std::uint16_t registerIndex;
    std::uint16_t registerCount;
    std::uint16_t reserved;
    std::uint32_t typeInfo;
    std::uint32_t defaultValue;
};

std::vector<ShaderConstant> ParseCtab(const std::uint8_t* data, std::size_t size) {
    std::vector<ShaderConstant> result;
    if (size < sizeof(CtabHeader)) {
        return result;
    }
    CtabHeader header{};
    std::memcpy(&header, data, sizeof(header));
    if (header.constantInfo > size || header.constants > (size - header.constantInfo) / sizeof(CtabConstantInfo)) {
        return result;
    }
    for (std::uint32_t i = 0; i < header.constants; ++i) {
        CtabConstantInfo info{};
        std::memcpy(&info, data + header.constantInfo + i * sizeof(CtabConstantInfo), sizeof(info));
        ShaderConstant constant;
        constant.registerSet = info.registerSet;
        constant.registerIndex = info.registerIndex;
        constant.registerCount = info.registerCount;
        if (info.name < size) {
            const auto* name = reinterpret_cast<const char*>(data + info.name);
            constant.name.assign(name, strnlen(name, size - info.name));
        }
        result.push_back(std::move(constant));
    }
    return result;
}

}  // namespace

std::vector<ShaderConstant> ParseConstantTable(const std::uint32_t* tokens, std::size_t tokenCount) {
    if (tokens == nullptr || tokenCount < 2) {
        return {};
    }
    // Le premier jeton est la version (ex. 0xFFFE0300 pour vs_3_0) : les commentaires suivent.
    std::size_t position = 1;
    while (position < tokenCount && tokens[position] != kEndToken) {
        const std::uint32_t token = tokens[position];
        if ((token & 0xFFFF) != kCommentToken) {
            break;  // la table, quand elle existe, précède les instructions
        }
        const std::size_t length = (token >> 16) & 0x7FFF;
        if (position + 1 + length > tokenCount) {
            break;
        }
        if (length >= 1 && tokens[position + 1] == kCtabFourCc) {
            return ParseCtab(reinterpret_cast<const std::uint8_t*>(&tokens[position + 2]), (length - 1) * 4);
        }
        position += 1 + length;
    }
    return {};
}

}  // namespace hca::graphics

#include "PreparedPresetData.hpp"

#include "MilkdropShader.hpp"

namespace libprojectM {
namespace MilkdropPreset {

PreparedPresetData::PreparedPresetData() = default;
PreparedPresetData::~PreparedPresetData() = default;
PreparedPresetData::PreparedPresetData(PreparedPresetData&& other) noexcept = default;
PreparedPresetData& PreparedPresetData::operator=(PreparedPresetData&& other) noexcept = default;

} // namespace MilkdropPreset
} // namespace libprojectM

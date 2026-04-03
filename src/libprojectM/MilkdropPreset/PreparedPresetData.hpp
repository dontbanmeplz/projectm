/**
 * @file PreparedPresetData.hpp
 * @brief Data struct holding the results of Phase 1 (background thread) preset preparation.
 *
 * This struct contains all data parsed and transpiled from a .milk preset file.
 * It is created on a background thread without requiring a GL context,
 * then consumed on the GL thread to finalize preset loading with minimal blocking.
 */
#pragma once

#include "PresetFileParser.hpp"

#include <memory>
#include <string>

namespace libprojectM {
namespace MilkdropPreset {

class MilkdropShader;

/**
 * @brief Holds all data parsed/transpiled from a .milk preset file.
 *
 * Created on a background thread (no GL context required).
 * Consumed by MilkdropPreset constructor and InitializeFromPreparedData() on the GL thread.
 */
struct PreparedPresetData {
    PreparedPresetData();
    ~PreparedPresetData();

    // Move-only
    PreparedPresetData(PreparedPresetData&& other) noexcept;
    PreparedPresetData& operator=(PreparedPresetData&& other) noexcept;

    PreparedPresetData(const PreparedPresetData&) = delete;
    PreparedPresetData& operator=(const PreparedPresetData&) = delete;

    /// The original file path used to load the preset.
    std::string filePath;

    /// Parsed preset file data (raw key-value pairs from the .milk file).
    PresetFileParser parsedFile;

    /// Transpiled GLSL fragment shader source for the warp shader.
    /// Empty if the preset does not use a warp shader.
    std::string warpFragmentGLSL;

    /// Transpiled GLSL fragment shader source for the composite shader.
    /// Empty if the preset does not use a composite shader (Milkdrop 1.x style).
    std::string compFragmentGLSL;

    /// Pre-created warp shader object with metadata intact
    /// (m_samplerNames, m_maxBlurLevelRequired, m_randValues, etc.).
    /// Ownership is transferred to the preset on the GL thread.
    std::unique_ptr<MilkdropShader> warpShader;

    /// Pre-created composite shader object with metadata intact.
    /// Ownership is transferred to the preset on the GL thread.
    std::unique_ptr<MilkdropShader> compShader;

    /// True if the composite shader code was invalid and a fallback default was used.
    bool usedCompositeFallback{false};

    /// True if Phase 1 preparation succeeded. If false, the GL thread should
    /// either fall back to synchronous loading or report the error.
    bool valid{false};

    /// Error message if preparation failed. Empty on success.
    std::string errorMessage;
};

} // namespace MilkdropPreset
} // namespace libprojectM

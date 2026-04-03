//
// C++ Interface: MilkdropPresetFactory
//
// Description:
//
//
// Author: Carmelo Piccione <carmelo.piccione@gmail.com>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//

#pragma once

#include "PreparedPresetData.hpp"

#include <PresetFactory.hpp>
#include <Renderer/RenderContext.hpp>

#include <memory>

namespace libprojectM {
namespace MilkdropPreset {

class Factory : public PresetFactory
{

public:
    std::unique_ptr<Preset> LoadPresetFromFile(const std::string& filename) override;

    std::unique_ptr<Preset> LoadPresetFromStream(std::istream& data) override;

    /**
     * @brief Prepares preset data from a file without requiring a GL context.
     *
     * Performs Phase 1 of async preset loading: parses the .milk file, preprocesses
     * HLSL shader code, and transpiles HLSL→GLSL. Can be called from any thread.
     *
     * Does not support "idle://" presets (returns invalid PreparedPresetData).
     *
     * @param filename The preset filename or URL to prepare.
     * @return Prepared preset data ready for Phase 2 GL loading, or invalid data on error.
     */
    static std::unique_ptr<PreparedPresetData> PreparePresetFromFile(const std::string& filename);

    /**
     * @brief Creates a Preset instance from PreparedPresetData (GL thread only).
     *
     * Constructs a MilkdropPreset from the already-parsed and transpiled data,
     * and initializes it with the given render context. This completes Phase 2.
     *
     * @param data The prepared data from Phase 1. Modified: shader objects are moved out.
     * @param renderContext The render context for GL resource initialization.
     * @return A valid preset pointer, or throws on error.
     */
    static std::unique_ptr<Preset> CreatePresetFromPreparedData(
        PreparedPresetData& data,
        const Renderer::RenderContext& renderContext);

    std::string supportedExtensions() const override
    {
        return ".milk .prjm";
    }

};

} // namespace MilkdropPreset
} // namespace libprojectM

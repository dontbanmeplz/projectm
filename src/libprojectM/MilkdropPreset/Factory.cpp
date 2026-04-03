#include "Factory.hpp"

#include "BlurTexture.hpp"
#include "IdlePreset.hpp"
#include "MilkdropPreset.hpp"
#include "MilkdropShader.hpp"
#include "PresetFileParser.hpp"
#include "PresetState.hpp"

#include <Logging.hpp>

namespace libprojectM {
namespace MilkdropPreset {

std::unique_ptr<::libprojectM::Preset> Factory::LoadPresetFromFile(const std::string& filename)
{
    std::string path;
    auto protocol = PresetFactory::Protocol(filename, path);
    if (protocol == "idle")
    {
        return IdlePresets::allocate();
    }
    else if (protocol == "" || protocol == "file")
    {
        return std::make_unique<MilkdropPreset>(path);
    }
    else
    {
        // ToDO: Throw unsupported protocol exception instead to provide more information.
        return nullptr;
    }
}

std::unique_ptr<Preset> Factory::LoadPresetFromStream(std::istream& data)
{
    return std::make_unique<MilkdropPreset>(data);
}

std::unique_ptr<Preset> Factory::CreatePresetFromPreparedData(
    PreparedPresetData& data,
    const Renderer::RenderContext& renderContext)
{
    auto preset = std::make_unique<MilkdropPreset>(data);
    preset->InitializeFromPreparedData(renderContext, data);
    return preset;
}

std::unique_ptr<PreparedPresetData> Factory::PreparePresetFromFile(const std::string& filename)
{
    auto data = std::make_unique<PreparedPresetData>();
    data->filePath = filename;

    try
    {
        // Handle URL schemes
        std::string path;
        auto protocol = PresetFactory::Protocol(filename, path);

        if (protocol == "idle")
        {
            // Idle presets can't be prepared asynchronously (they're special hardcoded presets)
            data->errorMessage = "Async preparation not supported for idle presets";
            return data;
        }

        if (!protocol.empty() && protocol != "file")
        {
            data->errorMessage = "Unsupported protocol: " + protocol;
            return data;
        }

        // Phase 1a: Parse the .milk file (file I/O only)
        if (!data->parsedFile.Read(path))
        {
            data->errorMessage = "Could not parse preset file: " + path;
            return data;
        }

        // Phase 1b: Initialize a temporary PresetState from parsed values (CPU only)
        PresetState tempState;
        tempState.Initialize(data->parsedFile);

        // Phase 1c: Load and preprocess warp shader code (CPU only)
        if (tempState.warpShaderVersion > 0 && !tempState.warpShader.empty())
        {
            try
            {
                data->warpShader = std::make_unique<MilkdropShader>(MilkdropShader::ShaderType::WarpShader);
                data->warpShader->LoadCode(tempState.warpShader);

                // Generate stub declarations and transpile
                std::set<std::string> samplerDecls;
                std::set<std::string> texSizeDecls;
                data->warpShader->GenerateStubDeclarations(samplerDecls, texSizeDecls, tempState.blurTexture);
                data->warpFragmentGLSL = data->warpShader->TranspileToGLSL(samplerDecls, texSizeDecls);
            }
            catch (const Renderer::ShaderException& ex)
            {
                LOG_ERROR("[Factory::PreparePresetFromFile] Error preparing warp shader: " + ex.message());
                data->warpShader.reset();
                data->warpFragmentGLSL.clear();
            }
        }

        // Phase 1d: Load and preprocess composite shader code (CPU only)
        static const std::string defaultCompositeShader = "shader_body\n{\nret = tex2D(sampler_main, uv).xyz;\n}";

        if (tempState.compositeShaderVersion > 0)
        {
            data->compShader = std::make_unique<MilkdropShader>(MilkdropShader::ShaderType::CompositeShader);

            if (!tempState.compositeShader.empty())
            {
                try
                {
                    data->compShader->LoadCode(tempState.compositeShader);
                }
                catch (const Renderer::ShaderException& ex)
                {
                    LOG_WARN("[Factory::PreparePresetFromFile] Error loading composite shader: " + ex.message() + " - Using fallback.");
                    data->compShader = std::make_unique<MilkdropShader>(MilkdropShader::ShaderType::CompositeShader);
                    data->compShader->LoadCode(defaultCompositeShader);
                    data->usedCompositeFallback = true;
                }
            }
            else
            {
                data->compShader->LoadCode(defaultCompositeShader);
            }

            // Transpile the composite shader
            try
            {
                std::set<std::string> samplerDecls;
                std::set<std::string> texSizeDecls;
                data->compShader->GenerateStubDeclarations(samplerDecls, texSizeDecls, tempState.blurTexture);
                data->compFragmentGLSL = data->compShader->TranspileToGLSL(samplerDecls, texSizeDecls);
            }
            catch (const Renderer::ShaderException& ex)
            {
                LOG_WARN("[Factory::PreparePresetFromFile] Error transpiling composite shader: " + ex.message() + " - Using fallback.");
                data->compShader = std::make_unique<MilkdropShader>(MilkdropShader::ShaderType::CompositeShader);
                data->compShader->LoadCode(defaultCompositeShader);
                data->usedCompositeFallback = true;

                std::set<std::string> samplerDecls;
                std::set<std::string> texSizeDecls;
                data->compShader->GenerateStubDeclarations(samplerDecls, texSizeDecls, tempState.blurTexture);
                data->compFragmentGLSL = data->compShader->TranspileToGLSL(samplerDecls, texSizeDecls);
            }
        }
        // else: Milkdrop 1.x style — no composite shader, compShader stays null

        data->valid = true;
    }
    catch (const std::exception& ex)
    {
        data->errorMessage = ex.what();
        LOG_ERROR("[Factory::PreparePresetFromFile] Exception: " + std::string(ex.what()));
    }
    catch (...)
    {
        data->errorMessage = "Unknown exception during preset preparation";
        LOG_ERROR("[Factory::PreparePresetFromFile] Unknown exception");
    }

    return data;
}

} // namespace MilkdropPreset
} // namespace libprojectM

/**
 * @file MilkdropShader.hpp
 * @brief Holds a warp or composite shader of Milkdrop presets.
 *
 * This class wraps the conversion from HLSL shader code to GLSL and also manages the
 * drawing.
 */
#pragma once

#include "BlurTexture.hpp"

#include <Renderer/Shader.hpp>
#include <Renderer/TextureManager.hpp>

#include <array>
#include <set>

namespace libprojectM {
namespace MilkdropPreset {

class PerFrameContext;
class PresetState;

/**
 * @brief Holds a warp or composite shader of Milkdrop presets.
 * Also does the required shader translation from HLSL to GLSL using hlslparser.
 */
class MilkdropShader
{
public:
    enum class ShaderType
    {
        WarpShader,     //!< Warp shader
        CompositeShader //!< Composite shader
    };

    /**
     * constructor.
     * @param type The preset shader type.
     */
    explicit MilkdropShader(ShaderType type);

    /**
     * @brief Translates and compiles the shader code.
     * @param presetShaderCode The preset shader code.
     */
    void LoadCode(const std::string& presetShaderCode);

    /**
     * @brief Loads the required texture references into the shader.
     * Binds the underlying shader program.
     * @param presetState The preset state to pull the values and textures from.
     */
    void LoadTexturesAndCompile(PresetState& presetState);

    /**
     * @brief Loads all required shader variables into the uniforms.
     * Binds the underlying shader program.
     * @param presetState The preset state to pull the values from.
     * @param perFrameContext The per-frame context with dynamically calculated values.
     */
    void LoadVariables(const PresetState& presetState, const PerFrameContext& perFrameContext);

    /**
     * @brief Returns the contained shader.
     * @return The shader program wrapper.
     */
    auto Shader() -> Renderer::Shader&;

    /**
     * @brief Generates stub sampler and texsize declarations from sampler names only (no GL context).
     *
     * Uses the sampler names extracted by LoadCode() to produce the same HLSL declaration strings
     * that TranspileHLSLShader() would normally get from loaded TextureSamplerDescriptors.
     * Only "noisevol_lq" and "noisevol_hq" are declared as sampler3D; all others are sampler2D.
     *
     * @param outSamplerDecls Set to receive sampler uniform declarations.
     * @param outTexSizeDecls Set to receive texsize uniform declarations.
     * @param blurTexture A BlurTexture instance (may be default-constructed) for blur descriptor declarations.
     */
    void GenerateStubDeclarations(
        std::set<std::string>& outSamplerDecls,
        std::set<std::string>& outTexSizeDecls,
        const BlurTexture& blurTexture) const;

    /**
     * @brief Transpiles the preprocessed HLSL shader to GLSL without compiling (no GL context required).
     *
     * Takes pre-built sampler/texsize declarations (from GenerateStubDeclarations or from loaded textures)
     * and returns the transpiled GLSL fragment shader source string. Does NOT call CompileProgram().
     *
     * @param samplerDecls Sampler uniform declarations to prepend to the HLSL source.
     * @param texSizeDecls Texsize uniform declarations to prepend to the HLSL source.
     * @return The transpiled GLSL fragment shader source code.
     * @throws Renderer::ShaderException if transpilation fails.
     */
    std::string TranspileToGLSL(
        const std::set<std::string>& samplerDecls,
        const std::set<std::string>& texSizeDecls);

    /**
     * @brief Loads textures and compiles the shader from pre-transpiled GLSL (GL context required).
     *
     * Resolves sampler names to actual GL textures via TextureManager, then compiles the shader
     * program using the provided pre-transpiled GLSL fragment source.
     *
     * @param presetState The preset state for texture and blur level access.
     * @param fragmentGLSL Pre-transpiled GLSL fragment shader source.
     */
    void LoadTexturesAndCompileFromPrepared(
        PresetState& presetState,
        const std::string& fragmentGLSL);

private:
    /**
     * @brief Prepares the shader code to be translated into GLSL.
     * @param program The program code to work on.
     */
    void PreprocessPresetShader(std::string& program);

    /**
     * @brief Searches for sampler references in the program and stores them in m_samplerNames.
     * @param program The program code to work on.
     */
    void GetReferencedSamplers(const std::string& program);

    /**
     * @brief Translates the HLSL shader into GLSL.
     * @param presetState The preset state to pull the blur textures from.
     * @param program The shader to transpile.
     */
    void TranspileHLSLShader(const PresetState& presetState, std::string& program);

    /**
     * @brief Updates the requested blur level if higher than before.
     * Also adds the required samplers.
     * @param requestedLevel The requested blur level.
     */
    void UpdateMaxBlurLevel(BlurTexture::BlurLevel requestedLevel);

    ShaderType m_type{ShaderType::WarpShader}; //!< Type of this shader.
    std::string m_fragmentShaderCode;          //!< The original preset fragment shader code.
    std::string m_preprocessedCode;            //!< The preprocessed preset shader code.

    std::set<std::string> m_samplerNames;                                        //!< All sampler names referenced in the shader code.
    std::vector<Renderer::TextureSamplerDescriptor> m_mainTextureDescriptors;              //!< Descriptors for all main texture references.
    std::vector<Renderer::TextureSamplerDescriptor> m_textureSamplerDescriptors;           //!< Descriptors of all referenced samplers in the shader code.
    BlurTexture::BlurLevel m_maxBlurLevelRequired{BlurTexture::BlurLevel::None}; //!< Max blur level of main texture required by this shader.

    std::array<float, 4> m_randValues{};               //!< Random values which don't change every frame.
    std::array<glm::vec3, 20> m_randTranslation{};     //!< Random translation vectors which don't change every frame.
    std::array<glm::vec3, 20> m_randRotationCenters{}; //!< Random rotation center vectors which don't change every frame.
    std::array<glm::vec3, 20> m_randRotationSpeeds{};  //!< Random rotation speeds which don't change every frame.

    Renderer::Shader m_shader;
};

} // namespace MilkdropPreset
} // namespace libprojectM

// Diagnostic of real Ogre GL3Plus allocation, transfers and shader sampling.
// The root agent runs this with GPU access; compilation alone is not a PASS.
#include <Ogre.h>
#include <OgreGL3PlusPlugin.h>
#include <OgreGL3PlusTexture.h>
#include <OgreGL3PlusTextureBuffer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    constexpr unsigned Edge = 4;
    using Rgba = std::array<float, 4>;
    int checks = 0, failures = 0;
    std::size_t glErrors = 0, startupErrors = 0, textureErrors = 0;
    bool startupComplete = false;

    void require(bool value, const std::string& reason)
    {
        if (!value) throw std::runtime_error(reason);
    }

    void check(const std::string& name, bool value)
    {
        ++checks;
        if (!value) ++failures;
        std::cout << "[GL3PLUS_TEXTURE_GPU] " << (value ? "PASS " : "FAIL ") << name << '\n';
    }

    void glStage(const std::string& name)
    {
        bool clean = true;
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            clean = false;
            ++glErrors;
            if (startupComplete) ++textureErrors;
            else ++startupErrors;
            std::cout << "[GL3PLUS_TEXTURE_GL] stage=" << name << " error=0x"
                      << std::hex << error << std::dec << '\n';
        }
        if (clean) std::cout << "[GL3PLUS_TEXTURE_GL] stage=" << name << " error=0\n";
    }

    // Restore actual driver state around the independent shader oracle, including
    // bindings touched by Ogre's cached state. No production sampler is changed.
    class GlState final
    {
      public:
        GlState()
        {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &activeBinding);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureZeroBinding);
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
            glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
            glGetIntegerv(GL_PACK_ROW_LENGTH, &packRowLength);
            glGetIntegerv(GL_PACK_SKIP_ROWS, &packSkipRows);
            glGetIntegerv(GL_PACK_SKIP_PIXELS, &packSkipPixels);
            glGetIntegerv(GL_VIEWPORT, viewport.data());
            for (std::size_t i = 0; i < capabilities.size(); ++i)
                enabled[i] = glIsEnabled(capabilities[i]);
        }

        ~GlState()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer));
            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer));
            glUseProgram(static_cast<GLuint>(program));
            glBindVertexArray(static_cast<GLuint>(vao));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textureZeroBinding));
            glActiveTexture(static_cast<GLenum>(activeTexture));
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(activeBinding));
            glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(packBuffer));
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpackBuffer));
            glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
            glPixelStorei(GL_PACK_ROW_LENGTH, packRowLength);
            glPixelStorei(GL_PACK_SKIP_ROWS, packSkipRows);
            glPixelStorei(GL_PACK_SKIP_PIXELS, packSkipPixels);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            for (std::size_t i = 0; i < capabilities.size(); ++i)
            {
                if (enabled[i]) glEnable(capabilities[i]);
                else glDisable(capabilities[i]);
            }
        }

      private:
        GLint drawFramebuffer = 0, readFramebuffer = 0, program = 0, vao = 0;
        GLint activeTexture = 0, activeBinding = 0, textureZeroBinding = 0;
        GLint packBuffer = 0, unpackBuffer = 0;
        GLint packAlignment = 0, packRowLength = 0, packSkipRows = 0, packSkipPixels = 0;
        std::array<GLint, 4> viewport{};
        const std::array<GLenum, 6> capabilities{{GL_BLEND, GL_DEPTH_TEST, GL_CULL_FACE,
                                                GL_SCISSOR_TEST, GL_FRAMEBUFFER_SRGB, GL_DITHER}};
        std::array<GLboolean, 6> enabled{};
    };

    GLuint shader(GLenum type, const char* source)
    {
        const GLuint value = glCreateShader(type);
        glShaderSource(value, 1, &source, nullptr);
        glCompileShader(value);
        GLint success = 0;
        glGetShaderiv(value, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            std::array<char, 4096> log{};
            glGetShaderInfoLog(value, static_cast<GLsizei>(log.size()), nullptr, log.data());
            glDeleteShader(value);
            throw std::runtime_error(std::string("Independent shader compile: ") + log.data());
        }
        return value;
    }

    class SampleProbe final
    {
      public:
        SampleProbe()
        {
            GlState restore;
            const GLuint vertex = shader(GL_VERTEX_SHADER,
                "#version 150\n"
                "void main(){ vec2 p=vec2(gl_VertexID==1?3.0:-1.0,"
                "gl_VertexID==2?3.0:-1.0); gl_Position=vec4(p,0.0,1.0); }\n");
            const GLuint fragment = shader(GL_FRAGMENT_SHADER,
                "#version 150\n"
                "uniform sampler2D inputTexture; uniform int inputLevel; out vec4 outputColor;\n"
                "void main(){ outputColor=texelFetch(inputTexture,ivec2(gl_FragCoord.xy),inputLevel); }\n");
            program = glCreateProgram();
            glAttachShader(program, vertex);
            glAttachShader(program, fragment);
            glBindFragDataLocation(program, 0, "outputColor");
            glLinkProgram(program);
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            GLint success = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            require(success != 0, "Independent shader link failed");
            glGenVertexArrays(1, &vao);
            glGenTextures(1, &target);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, target);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, Edge, Edge, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                    "Independent RGBA32F sample framebuffer is incomplete");
        }

        ~SampleProbe()
        {
            glDeleteFramebuffers(1, &framebuffer);
            glDeleteTextures(1, &target);
            glDeleteVertexArrays(1, &vao);
            glDeleteProgram(program);
        }

        std::vector<Rgba> sample(const Ogre::TexturePtr& texture, unsigned level = 0)
        {
            GlState restore;
            auto* actual = dynamic_cast<Ogre::GL3PlusTexture*>(texture.getPointer());
            require(actual != nullptr, "Texture was not allocated by actual GL3Plus");
            const unsigned width = std::max(1u,texture->getWidth()>>level);
            const unsigned height = std::max(1u,texture->getHeight()>>level);
            require(width <= Edge && height <= Edge, "Sample probe bound exceeded");
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            for (const GLenum capability : {GL_BLEND, GL_DEPTH_TEST, GL_CULL_FACE,
                                           GL_SCISSOR_TEST, GL_FRAMEBUFFER_SRGB, GL_DITHER})
                glDisable(capability);
            glViewport(0, 0, width, height);
            glUseProgram(program);
            glUniform1i(glGetUniformLocation(program, "inputTexture"), 0);
            glUniform1i(glGetUniformLocation(program, "inputLevel"), static_cast<GLint>(level));
            glBindVertexArray(vao);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, actual->getGLID());
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_PACK_ROW_LENGTH, 0);
            glPixelStorei(GL_PACK_SKIP_ROWS, 0);
            glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
            std::vector<Rgba> result(width * height);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, result.data());
            return result;
        }

      private:
        GLuint program = 0, vao = 0, framebuffer = 0, target = 0;
    };

    class Texture final
    {
      private:
        std::string name;
      public:
        Texture(const std::string& name, Ogre::PixelFormat format, unsigned edge = Edge, int mipmaps = 0,
                int usage = Ogre::TU_STATIC_WRITE_ONLY, Ogre::TextureType type = Ogre::TEX_TYPE_2D,
                unsigned depth = 1)
            : name("GL3PlusTextureOracle/" + name), value(Ogre::TextureManager::getSingleton().createManual(
                  this->name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                  type, edge, edge, depth, mipmaps, format, usage))
        {
            glStage("Ogre create " + name);
        }
        ~Texture()
        {
            Ogre::TextureManager::getSingleton().remove(name);
            value.setNull();
        }
        Ogre::TexturePtr value;
    };

    bool close(const std::vector<Rgba>& actual, const std::vector<Rgba>& expected,
               const Rgba& tolerance)
    {
        if (actual.size() != expected.size()) return false;
        for (std::size_t pixel = 0; pixel < actual.size(); ++pixel)
            for (std::size_t channel = 0; channel < 4; ++channel)
                if (!std::isfinite(actual[pixel][channel]) ||
                    std::abs(actual[pixel][channel] - expected[pixel][channel]) > tolerance[channel])
                {
                    std::cout << "[GL3PLUS_TEXTURE_DIFFERENCE] pixel=" << pixel << " channel=" << channel
                              << " actual=" << actual[pixel][channel] << " expected=" << expected[pixel][channel]
                              << " tolerance=" << tolerance[channel] << '\n';
                    return false;
                }
        return true;
    }

    std::vector<Rgba> alphaSamples(const std::vector<std::uint8_t>& bytes)
    {
        std::vector<Rgba> expected;
        for (const auto alpha : bytes) expected.push_back({0, 0, 0, alpha / 255.f});
        return expected;
    }

    void alphaCases(SampleProbe& probe)
    {
        Texture texture("A8", Ogre::PF_A8);
        const auto buffer = texture.value->getBuffer();
        check("A8 allocation retains alpha texture and surface semantics",
              texture.value->getFormat() == Ogre::PF_A8 && buffer->getFormat() == Ogre::PF_A8);
        std::vector<std::uint8_t> alpha{0,64,128,255, 17,193,91,227, 31,151,219,79, 241,47,109,181};
        buffer->blitFromMemory(Ogre::PixelBox(Edge, Edge, 1, Ogre::PF_A8, alpha.data()));
        glStage("A8 varied-byte upload");
        std::vector<std::uint8_t> downloaded(alpha.size(), 99);
        buffer->blitToMemory(Ogre::PixelBox(Edge, Edge, 1, Ogre::PF_A8, downloaded.data()));
        glStage("A8 full native readback");
        check("A8 full native readback preserves varied alpha bytes", downloaded == alpha);
        check("A8 shader samples zero RGB and stored alpha", close(probe.sample(texture.value),
              alphaSamples(alpha), {1e-6f,1e-6f,1e-6f,1e-6f}));
        glStage("A8 initial shader sample");

        std::array<std::uint8_t, 4> patch{{255,0,64,128}};
        buffer->blitFromMemory(Ogre::PixelBox(2, 2, 1, Ogre::PF_A8, patch.data()), Ogre::Box(1,1,3,3));
        for (unsigned y = 0; y < 2; ++y)
            for (unsigned x = 0; x < 2; ++x) alpha[(y+1)*Edge+x+1] = patch[y*2+x];
        glStage("A8 subupload");
        std::array<std::uint8_t, 4> cropped{};
        buffer->blitToMemory(Ogre::Box(1,1,3,3), Ogre::PixelBox(2,2,1,Ogre::PF_A8,cropped.data()));
        glStage("A8 cropped native readback");
        check("A8 cropped readback uses alpha surface conversion", cropped == patch);
        check("A8 subupload changes exactly intended shader texels", close(probe.sample(texture.value),
              alphaSamples(alpha), {1e-6f,1e-6f,1e-6f,1e-6f}));
        glStage("A8 patched shader sample");

        std::array<std::uint8_t, 4> small{{0,64,128,255}};
        buffer->blitFromMemory(Ogre::PixelBox(2,2,1,Ogre::PF_A8,small.data()), Ogre::Box(0,0,Edge,Edge));
        glStage("A8 real two-to-four resize upload");
        const auto resized = probe.sample(texture.value);
        bool corners = true, channels = true, varied = false;
        const std::array<unsigned, 4> cornerIndices{{0,3,12,15}};
        for (unsigned i = 0; i < 4; ++i)
            corners &= std::abs(resized[cornerIndices[i]][3] - small[i]/255.f) <= 1.f/255;
        for (const auto& pixel : resized)
        {
            channels &= pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0 && pixel[3] >= 0 && pixel[3] <= 1;
            varied |= pixel[3] > 0.05f && pixel[3] < 0.95f;
        }
        check("A8 resize preserves independent corner alpha and intermediate samples", corners && channels && varied);
        glStage("A8 resized shader sample");
        buffer->blitToMemory(Ogre::Box(1,1,3,3), Ogre::PixelBox(2,2,1,Ogre::PF_A8,cropped.data()));
        glStage("A8 resized cropped readback");
        bool cropMatches = true;
        for (unsigned y = 0; y < 2; ++y)
            for (unsigned x = 0; x < 2; ++x)
                cropMatches &= std::abs(cropped[y*2+x]/255.f - resized[(y+1)*Edge+x+1][3]) <= 1.f/255;
        check("A8 resized cropped bytes agree with actual alpha shader samples", cropMatches);

        // A8 is a semantic alpha channel, including conversions. Distinct red
        // and alpha bytes prevent a raw red-storage transfer from passing.
        std::vector<std::uint8_t> rgba(Edge*Edge*4);
        for (unsigned i = 0; i < Edge*Edge; ++i)
        {
            rgba[i*4] = static_cast<std::uint8_t>((i*13+191)%256);
            rgba[i*4+1] = 83; rgba[i*4+2] = 211;
            alpha[i] = static_cast<std::uint8_t>((i*37+64)%256);
            rgba[i*4+3] = alpha[i];
        }
        buffer->blitFromMemory(Ogre::PixelBox(Edge,Edge,1,Ogre::PF_BYTE_RGBA,rgba.data()));
        glStage("RGBA distinct-red-alpha to A8 upload");
        check("RGBA to A8 upload extracts alpha rather than red",close(probe.sample(texture.value),
              alphaSamples(alpha),{1e-6f,1e-6f,1e-6f,1e-6f}));
        glStage("A8 cross-format upload shader sample");
        std::vector<std::uint8_t> expectedRgba(rgba.size(),0), downloadedRgba(rgba.size(),99);
        for (unsigned i = 0; i < Edge*Edge; ++i) expectedRgba[i*4+3] = alpha[i];
        buffer->blitToMemory(Ogre::PixelBox(Edge,Edge,1,Ogre::PF_BYTE_RGBA,downloadedRgba.data()));
        glStage("A8 full RGBA readback");
        check("A8 full cross-format readback returns zero RGB and stored alpha",downloadedRgba==expectedRgba);
        std::array<std::uint8_t,16> croppedRgba{};
        buffer->blitToMemory(Ogre::Box(1,1,3,3),Ogre::PixelBox(2,2,1,Ogre::PF_BYTE_RGBA,croppedRgba.data()));
        glStage("A8 cropped RGBA readback");
        bool convertedCrop = true;
        for (unsigned y = 0; y < 2; ++y)
            for (unsigned x = 0; x < 2; ++x)
                for (unsigned c = 0; c < 4; ++c)
                    convertedCrop &= croppedRgba[(y*2+x)*4+c]==expectedRgba[((y+1)*Edge+x+1)*4+c];
        check("A8 cropped cross-format readback preserves alpha semantics",convertedCrop);

        std::vector<std::uint8_t> pitchedSource(8*4,71);
        for (unsigned y = 0; y < 2; ++y)
            for (unsigned x = 0; x < 3; ++x)
            {
                const auto value = static_cast<std::uint8_t>(23+y*91+x*47);
                pitchedSource[(y+1)*8+x+2] = value;
                alpha[(y+1)*Edge+x] = value;
            }
        const Ogre::PixelBox nativeBacking(8,4,1,Ogre::PF_A8,pitchedSource.data());
        buffer->blitFromMemory(nativeBacking.getSubVolume(Ogre::Box(2,1,5,3),false),Ogre::Box(0,1,3,3));
        glStage("A8 nonzero-origin pitched native source upload");
        check("A8 pitched source respects origin row pitch and destination subbox",
              close(probe.sample(texture.value),alphaSamples(alpha),{1e-6f,1e-6f,1e-6f,1e-6f}));
        glStage("A8 native pitched-source shader sample");

        std::vector<std::uint8_t> pitchedRgba(8*4*4,117);
        for (unsigned y = 0; y < 2; ++y)
            for (unsigned x = 0; x < 3; ++x)
            {
                const auto a = static_cast<std::uint8_t>(229-y*63-x*31);
                const unsigned i = ((y+1)*8+x+2)*4;
                pitchedRgba[i] = 7; pitchedRgba[i+1] = 91; pitchedRgba[i+2] = 153; pitchedRgba[i+3] = a;
                alpha[(y+1)*Edge+x] = a;
            }
        const Ogre::PixelBox rgbaBacking(8,4,1,Ogre::PF_BYTE_RGBA,pitchedRgba.data());
        buffer->blitFromMemory(rgbaBacking.getSubVolume(Ogre::Box(2,1,5,3),false),Ogre::Box(0,1,3,3));
        glStage("A8 nonzero-origin pitched RGBA source conversion");
        check("A8 pitched RGBA conversion extracts alpha at actual source offsets",
              close(probe.sample(texture.value),alphaSamples(alpha),{1e-6f,1e-6f,1e-6f,1e-6f}));
        glStage("A8 converted pitched-source shader sample");

        std::vector<std::uint8_t> pitchedDestination(8*4,165);
        const Ogre::PixelBox outputBacking(8,4,1,Ogre::PF_A8,pitchedDestination.data());
        buffer->blitToMemory(outputBacking.getSubVolume(Ogre::Box(2,0,6,4),false));
        glStage("A8 full native nonzero-origin pitched destination readback");
        bool pitchedOutput = true;
        for (unsigned y = 0; y < 4; ++y)
            for (unsigned x = 0; x < 8; ++x)
                pitchedOutput &= pitchedDestination[y*8+x]==(x>=2&&x<6?alpha[y*4+x-2]:165);
        check("A8 pitched native readback preserves pixels and surrounding canaries",pitchedOutput);

        std::vector<std::uint8_t> pitchedConverted(8*4*4,165);
        const Ogre::PixelBox convertedBacking(8,4,1,Ogre::PF_BYTE_RGBA,pitchedConverted.data());
        buffer->blitToMemory(Ogre::Box(1,1,3,3),convertedBacking.getSubVolume(Ogre::Box(2,1,4,3),false));
        glStage("A8 cropped RGBA nonzero-origin pitched destination readback");
        bool pitchedConvertedOutput = true;
        for (unsigned y = 0; y < 4; ++y)
            for (unsigned x = 0; x < 8; ++x)
                for (unsigned c = 0; c < 4; ++c)
                {
                    const bool inside = x>=2&&x<4&&y>=1&&y<3;
                    const unsigned expectedByte = inside ? (c==3?alpha[y*Edge+x-1]:0) : 165;
                    pitchedConvertedOutput &= pitchedConverted[(y*8+x)*4+c]==expectedByte;
                }
        check("A8 pitched cropped RGBA readback preserves alpha and surrounding canaries",pitchedConvertedOutput);

        // Constant nontrivial alpha removes filter-coordinate ambiguity from the
        // independent cross-format resize and texture-to-texture blit oracle.
        std::fill(alpha.begin(),alpha.end(),73);
        buffer->blitFromMemory(Ogre::PixelBox(Edge,Edge,1,Ogre::PF_A8,alpha.data()));
        std::fill(croppedRgba.begin(),croppedRgba.end(),0);
        buffer->blitToMemory(Ogre::Box(0,0,Edge,Edge),Ogre::PixelBox(2,2,1,Ogre::PF_BYTE_RGBA,croppedRgba.data()));
        glStage("A8 to RGBA resized memory readback");
        bool convertedResize = true;
        for (unsigned i = 0; i < 4; ++i)
            convertedResize &= croppedRgba[i*4]==0&&croppedRgba[i*4+1]==0&&
                               croppedRgba[i*4+2]==0&&croppedRgba[i*4+3]==73;
        check("A8 resized cross-format readback preserves zero RGB and nontrivial alpha",convertedResize);
        {
            Texture destination("A8-to-RGBA-blit",Ogre::PF_BYTE_RGBA,Edge,1,
                                Ogre::TU_STATIC_WRITE_ONLY|Ogre::TU_AUTOMIPMAP);
            destination.value->getBuffer()->blit(buffer,Ogre::Box(0,0,Edge,Edge),Ogre::Box(0,0,Edge,Edge));
            glStage("A8 to RGBA real texture-to-texture blit");
            check("A8 to RGBA texture blit preserves semantic alpha rather than storage red",
                  close(probe.sample(destination.value),alphaSamples(alpha),{1e-6f,1e-6f,1e-6f,1e-6f}));
            glStage("A8 to RGBA blit shader sample");
            check("A8 cross-format texture blit retains destination automatic mip generation",
                  close(probe.sample(destination.value,1),alphaSamples(std::vector<std::uint8_t>(4,73)),
                        {1e-6f,1e-6f,1e-6f,1e-6f}));
            glStage("A8 to RGBA blit automatic mip shader sample");
        }
        {
            Texture mipTexture("A8-mip",Ogre::PF_A8,Edge,1);
            std::vector<std::uint8_t> base(Edge*Edge,17);
            mipTexture.value->getBuffer(0,0)->blitFromMemory(Ogre::PixelBox(Edge,Edge,1,Ogre::PF_A8,base.data()));
            std::vector<std::uint8_t> mip{{3,97,199,251}}, mipRead(4,0);
            const auto mipBuffer = mipTexture.value->getBuffer(0,1);
            mipBuffer->blitFromMemory(Ogre::PixelBox(2,2,1,Ogre::PF_A8,mip.data()));
            glStage("A8 independent nonzero mip upload");
            mipBuffer->blitToMemory(Ogre::PixelBox(2,2,1,Ogre::PF_A8,mipRead.data()));
            glStage("A8 nonzero mip native readback");
            check("A8 mip-one readback uses its own level-sized transfer without base offset",
                  mipBuffer->getWidth()==2&&mipBuffer->getHeight()==2&&
                  mipBuffer->getFormat()==Ogre::PF_A8&&mipRead==mip);
            check("A8 independently uploaded mip-one has correct shader alpha",
                  close(probe.sample(mipTexture.value,1),alphaSamples(mip),{1e-6f,1e-6f,1e-6f,1e-6f}));
            glStage("A8 mip-one shader sample");
            mipTexture.value->getBuffer(0,0)->blitToMemory(Ogre::PixelBox(Edge,Edge,1,Ogre::PF_A8,downloaded.data()));
            glStage("A8 base level after mip-one transfer");
            check("A8 mip-one transfer leaves base-level alpha bytes unchanged",downloaded==base);
        }
        {
            Texture source("RGBA-mip-to-A8",Ogre::PF_BYTE_RGBA,Edge,1);
            std::vector<std::uint8_t> base(Edge*Edge*4,19);
            source.value->getBuffer(0,0)->blitFromMemory(Ogre::PixelBox(Edge,Edge,1,Ogre::PF_BYTE_RGBA,base.data()));
            std::array<std::uint8_t,16> mip{{191,17,89,37, 193,19,91,83,
                                           197,23,97,149, 199,29,101,211}};
            const auto mipBuffer = source.value->getBuffer(0,1);
            mipBuffer->blitFromMemory(Ogre::PixelBox(2,2,1,Ogre::PF_BYTE_RGBA,mip.data()));
            glStage("RGBA nonzero mip source upload");
            std::array<std::uint8_t,6> mipCrop{{165,165,165,165,165,165}};
            const Ogre::PixelBox mipCropBacking(3,2,1,Ogre::PF_A8,mipCrop.data());
            mipBuffer->blitToMemory(Ogre::Box(0,0,1,2),
                                   mipCropBacking.getSubVolume(Ogre::Box(1,0,2,2),false));
            glStage("RGBA mip-one cropped memory readback to pitched A8 destination");
            check("RGBA mip-one cropped A8 memory readback extracts alpha and preserves canaries",
                  mipCrop==std::array<std::uint8_t,6>{{165,37,165,165,149,165}});
            buffer->blit(mipBuffer,Ogre::Box(0,0,1,2),Ogre::Box(1,1,2,3));
            alpha[5] = 37; alpha[9] = 149;
            glStage("RGBA mip-one cropped texture blit to A8");
            check("RGBA mip-one cropped blit to A8 extracts current-level alpha at source offsets",
                  close(probe.sample(texture.value),alphaSamples(alpha),{1e-6f,1e-6f,1e-6f,1e-6f}));
            glStage("RGBA mip-one to A8 destination shader sample");
        }
        {
            // This functional 3D regression checks a partial-layer converted
            // upload and the untouched layer. Memory-overread safety additionally
            // depends on production transfer allocation using the source depth.
            Texture volume("RGBA-volume-from-A8",Ogre::PF_BYTE_RGBA,2,0,Ogre::TU_STATIC_WRITE_ONLY,
                           Ogre::TEX_TYPE_3D,2);
            std::vector<std::uint8_t> original(2*2*2*4,0);
            for (unsigned pixel = 0; pixel < 8; ++pixel)
            {
                original[pixel*4] = 31; original[pixel*4+1] = 67;
                original[pixel*4+2] = 113; original[pixel*4+3] = 157;
            }
            const auto volumeBuffer = volume.value->getBuffer();
            volumeBuffer->blitFromMemory(Ogre::PixelBox(2,2,2,Ogre::PF_BYTE_RGBA,original.data()));
            glStage("RGBA two-layer volume base upload");
            std::array<std::uint8_t,4> layer{{0,64,128,255}};
            volumeBuffer->blitFromMemory(Ogre::PixelBox(2,2,1,Ogre::PF_A8,layer.data()),Ogre::Box(0,0,1,2,2,2));
            glStage("A8 converted upload into one RGBA volume layer");
            for (unsigned pixel = 0; pixel < 4; ++pixel)
            {
                original[(pixel+4)*4] = original[(pixel+4)*4+1] = original[(pixel+4)*4+2] = 0;
                original[(pixel+4)*4+3] = layer[pixel];
            }
            std::vector<std::uint8_t> actual(original.size(),99);
            volumeBuffer->blitToMemory(Ogre::PixelBox(2,2,2,Ogre::PF_BYTE_RGBA,actual.data()));
            glStage("RGBA two-layer volume readback after converted partial upload");
            check("A8 converted partial volume upload preserves target alpha and untouched layer",actual==original);

            // Use the public production upload method with a native heap vector
            // of exactly one layer. An isolated instrumented build can detect a
            // source-depth regression here without relying on NEDPOOL staging.
            const std::array<std::uint8_t,16> expectedDirect{{190,61,143,5, 177,68,132,72,
                                                           164,75,121,139, 151,82,110,206}};
            std::vector<std::uint8_t> direct(expectedDirect.begin(),expectedDirect.end());
            auto* actualVolumeBuffer = dynamic_cast<Ogre::GL3PlusTextureBuffer*>(volumeBuffer.getPointer());
            require(actualVolumeBuffer != nullptr,"Volume surface was not actual GL3PlusTextureBuffer");
            actualVolumeBuffer->upload(Ogre::PixelBox(2,2,1,Ogre::PF_BYTE_RGBA,direct.data()),Ogre::Box(0,0,0,2,2,1));
            glStage("direct heap-backed RGBA single-layer volume upload");
            std::copy(expectedDirect.begin(),expectedDirect.end(),original.begin());
            volumeBuffer->blitToMemory(Ogre::PixelBox(2,2,2,Ogre::PF_BYTE_RGBA,actual.data()));
            glStage("volume readback after direct heap-backed partial-depth upload");
            check("direct heap-backed RGBA partial volume upload preserves input and untouched layer",actual==original);
        }
    }

    void luminanceCases(SampleProbe& probe)
    {
        for (const bool withAlpha : {false, true})
        {
            const std::string name = withAlpha ? "LA" : "L8";
            const auto format = withAlpha ? Ogre::PF_BYTE_LA : Ogre::PF_L8;
            Texture texture(name, format);
            std::vector<std::uint8_t> source(Edge*Edge*(withAlpha?2:1));
            std::vector<Rgba> expected;
            for (unsigned pixel = 0; pixel < Edge*Edge; ++pixel)
            {
                const auto luminance = static_cast<std::uint8_t>((pixel*53+17)%256);
                const auto alpha = static_cast<std::uint8_t>((pixel*29+71)%256);
                source[pixel*(withAlpha?2:1)] = luminance;
                if (withAlpha) source[pixel*2+1] = alpha;
                // Existing engine semantics are deliberately retained: L8's alpha
                // repeats L; LA's alpha comes from its second stored component.
                expected.push_back({luminance/255.f,luminance/255.f,luminance/255.f,
                                    (withAlpha?alpha:luminance)/255.f});
            }
            texture.value->getBuffer()->blitFromMemory(Ogre::PixelBox(Edge,Edge,1,format,source.data()));
            glStage(name + " native upload");
            std::vector<std::uint8_t> downloaded(source.size(), 0);
            texture.value->getBuffer()->blitToMemory(Ogre::PixelBox(Edge,Edge,1,format,downloaded.data()));
            glStage(name + " native readback");
            check(name + " semantic format and native bytes remain unchanged",
                  texture.value->getFormat() == format && downloaded == source);
            check(name + " existing luminance shader swizzle remains unchanged",
                  close(probe.sample(texture.value), expected, {1e-6f,1e-6f,1e-6f,1e-6f}));
            glStage(name + " shader sample");
        }
    }

    void packedCases(SampleProbe& probe)
    {
        // The channel triples, masks and denominators form an independent native
        // format oracle, rather than asking production GL3PlusPixelUtil for order.
        const std::array<std::array<unsigned,3>,16> components{{
            {{31,0,0}},{{0,63,0}},{{0,0,31}},{{7,41,23}},
            {{19,11,3}},{{2,58,29}},{{13,27,5}},{{31,63,31}},
            {{0,0,0}},{{26,35,9}},{{4,5,28}},{{18,52,12}},
            {{11,19,30}},{{23,61,1}},{{6,14,21}},{{29,3,16}}}};
        for (const bool blueHigh : {false, true})
        {
            const std::string name = blueHigh ? "B5G6R5" : "R5G6B5";
            const auto format = blueHigh ? Ogre::PF_B5G6R5 : Ogre::PF_R5G6B5;
            Texture texture(name, format);
            std::array<std::uint16_t,16> words{};
            std::vector<Rgba> expected;
            for (unsigned i = 0; i < words.size(); ++i)
            {
                const auto& c = components[i];
                words[i] = static_cast<std::uint16_t>((blueHigh?c[2]:c[0])<<11 | c[1]<<5 |
                                                      (blueHigh?c[0]:c[2]));
                expected.push_back({c[0]/31.f,c[1]/63.f,c[2]/31.f,1});
            }
            texture.value->getBuffer()->blitFromMemory(Ogre::PixelBox(Edge,Edge,1,format,words.data()));
            glStage(name + " asymmetric native packed upload");
            auto* actual = dynamic_cast<Ogre::GL3PlusTexture*>(texture.value.getPointer());
            require(actual != nullptr, "Packed texture is not GL3Plus");
            std::array<GLint,3> precision{};
            {
                GlState restore;
                glBindTexture(GL_TEXTURE_2D, actual->getGLID());
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_RED_SIZE,&precision[0]);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_GREEN_SIZE,&precision[1]);
                glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_BLUE_SIZE,&precision[2]);
            }
            glStage(name + " actual storage precision");
            std::cout << "[GL3PLUS_TEXTURE_PRECISION] format=" << name << " red=" << precision[0]
                      << " green=" << precision[1] << " blue=" << precision[2] << '\n';
            check(name + " actual storage has at least five bits per channel",
                  std::all_of(precision.begin(),precision.end(),[](GLint bits){ return bits>=5 && bits<=16; }));
            Rgba tolerance{{0,0,0,1e-6f}};
            for (unsigned channel = 0; channel < 3; ++channel)
                tolerance[channel] = 1.f/((1u<<std::clamp(precision[channel],1,16))-1) + 1e-6f;
            check(name + " shader preserves asymmetric RGB channel order at actual precision",
                  close(probe.sample(texture.value),expected,tolerance));
            glStage(name + " shader sample");
            std::array<std::uint16_t,16> downloaded{};
            texture.value->getBuffer()->blitToMemory(Ogre::PixelBox(Edge,Edge,1,format,downloaded.data()));
            glStage(name + " native packed readback");
            std::vector<Rgba> decoded;
            for (const auto word : downloaded)
            {
                const unsigned low = word&31, green = (word>>5)&63, high = (word>>11)&31;
                decoded.push_back({(blueHigh?low:high)/31.f,green/63.f,(blueHigh?high:low)/31.f,1});
            }
            tolerance[0] += 1.f/31; tolerance[1] += 1.f/63; tolerance[2] += 1.f/31;
            check(name + " native readback preserves channel order with storage quantization",
                  close(decoded,expected,tolerance));
        }
    }

    template<class T> float normalized(T value)
    {
        return std::max(-1.f,static_cast<float>(value)/std::numeric_limits<T>::max());
    }

    template<class T> void snormCase(SampleProbe& probe, Ogre::PixelFormat format, unsigned channels)
    {
        const std::string name = Ogre::PixelUtil::getFormatName(format);
        Texture texture(name,format);
        const std::array<T,5> levels{{std::numeric_limits<T>::min(),
            static_cast<T>(-std::numeric_limits<T>::max()/2),0,
            static_cast<T>(std::numeric_limits<T>::max()/2),std::numeric_limits<T>::max()}};
        std::vector<T> source(Edge*Edge*channels);
        for (unsigned i = 0; i < Edge*Edge; ++i)
            for (unsigned c = 0; c < channels; ++c) source[i*channels+c] = levels[(i+3*c)%levels.size()];
        auto expected = [&] {
            std::vector<Rgba> result(Edge*Edge,Rgba{{0,0,0,1}});
            for (unsigned i = 0; i < Edge*Edge; ++i)
                for (unsigned c = 0; c < channels; ++c) result[i][c] = normalized(source[i*channels+c]);
            return result;
        };
        const float epsilon = 2.f/std::numeric_limits<T>::max()+1e-6f;
        const Rgba tolerance{{epsilon,epsilon,epsilon,epsilon}};
        texture.value->getBuffer()->blitFromMemory(Ogre::PixelBox(Edge,Edge,1,format,source.data()));
        glStage(name + " min negative zero positive max upload");
        check(name + " semantic surface retains signed format", texture.value->getFormat() == format &&
              texture.value->getBuffer()->getFormat() == format);
        check(name + " float shader preserves signed range and channel order",
              close(probe.sample(texture.value),expected(),tolerance));
        glStage(name + " unclamped RGBA32F shader sample");
        std::vector<T> downloaded(source.size());
        texture.value->getBuffer()->blitToMemory(Ogre::PixelBox(Edge,Edge,1,format,downloaded.data()));
        glStage(name + " native signed readback");
        bool nativeMatches = true;
        for (unsigned i = 0; i < source.size(); ++i)
            nativeMatches &= std::abs(normalized(downloaded[i])-normalized(source[i]))<=epsilon;
        check(name + " native readback preserves signed normalized values",nativeMatches);
        std::vector<T> patch(2*channels);
        for (unsigned i = 0; i < patch.size(); ++i) patch[i] = levels[(i+1)%levels.size()];
        texture.value->getBuffer()->blitFromMemory(Ogre::PixelBox(1,2,1,format,patch.data()),Ogre::Box(2,1,3,3));
        for (unsigned y = 0; y < 2; ++y)
            for (unsigned c = 0; c < channels; ++c) source[((y+1)*Edge+2)*channels+c] = patch[y*channels+c];
        glStage(name + " signed subupload");
        check(name + " signed subupload preserves untouched and changed shader texels",
              close(probe.sample(texture.value),expected(),tolerance));
        glStage(name + " patched float shader sample");
    }

    void textureCases()
    {
        {
            SampleProbe probe;
            glStage("independent float shader oracle creation");
            alphaCases(probe);
            luminanceCases(probe);
            packedCases(probe);
            snormCase<std::int8_t>(probe,Ogre::PF_R8_SNORM,1);
            snormCase<std::int8_t>(probe,Ogre::PF_R8G8_SNORM,2);
            snormCase<std::int8_t>(probe,Ogre::PF_R8G8B8_SNORM,3);
            snormCase<std::int8_t>(probe,Ogre::PF_R8G8B8A8_SNORM,4);
            snormCase<std::int16_t>(probe,Ogre::PF_R16_SNORM,1);
            snormCase<std::int16_t>(probe,Ogre::PF_R16G16_SNORM,2);
            snormCase<std::int16_t>(probe,Ogre::PF_R16G16B16_SNORM,3);
            snormCase<std::int16_t>(probe,Ogre::PF_R16G16B16A16_SNORM,4);
        }
        glStage("all texture framebuffer program and VAO resources destroyed");
        check("texture operations and resource teardown retain zero GL errors",textureErrors==0);
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <new-ogre-log-path>\n";
        return 2;
    }
    try
    {
        const auto log = std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(log.parent_path());
        // Ogre Root is destroyed before the explicitly installed static plugin.
        auto plugin = std::make_unique<Ogre::GL3PlusPlugin>();
        auto root = std::make_unique<Ogre::Root>("","",log.string());
        root->installPlugin(plugin.get());
        Ogre::RenderSystem* renderSystem = nullptr;
        for (auto* candidate : root->getAvailableRenderers())
            if (candidate && candidate->getName().find("OpenGL 3+") != Ogre::String::npos)
                renderSystem = candidate;
        require(renderSystem != nullptr,"Actual GL3Plus render system unavailable");
        root->setRenderSystem(renderSystem);
        for (const auto& option : std::array<std::pair<Ogre::String,Ogre::String>,3>{{
                {"Full Screen","No"},{"VSync","No"},{"FSAA","0"}}})
            if (renderSystem->getConfigOptions().count(option.first))
                renderSystem->setConfigOption(option.first,option.second);
        root->initialise(false,"GL3Plus Texture Diagnostic");
        Ogre::NameValuePairList parameters;
        parameters["hidden"] = "true";
        parameters["noActivate"] = "true";
        auto* window = root->createRenderWindow("GL3Plus Texture Diagnostic",64,64,false,&parameters);
        require(window != nullptr,"Native hidden GL context creation failed");
        window->setAutoUpdated(false);
        window->setDeactivateOnFocusChange(false);
        glStage("Ogre startup hidden context");
        std::cout << "[GL3PLUS_TEXTURE_GPU] diagnostic=1 gameplay=0 hidden=1 noActivate=1"
                  << " GL_VENDOR=" << glGetString(GL_VENDOR) << " GL_RENDERER=" << glGetString(GL_RENDERER)
                  << " GL_VERSION=" << glGetString(GL_VERSION) << '\n';
        glStage("startup driver metadata");
        check("strict Ogre startup has zero GL errors",startupErrors==0);
        startupComplete = true;
        textureCases();
        // Context-dependent GL errors are checked after complete fixture resource
        // destruction, while the native context still exists. No GL API is used
        // after Root destroys that context; native shutdown exceptions remain fatal.
        root.reset();
        std::cout << "[GL3PLUS_TEXTURE_GPU] checks=" << checks << " failures=" << failures
                  << " gl_errors=" << glErrors << " startup_gl_errors=" << startupErrors
                  << " texture_gl_errors=" << textureErrors << '\n';
        return failures || glErrors ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[GL3PLUS_TEXTURE_GPU] FATAL " << error.what() << " checks=" << checks
                  << " failures=" << failures << " gl_errors=" << glErrors << '\n';
        return 2;
    }
}

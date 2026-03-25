#pragma once

#include "ResourceManager/ResourceManager.h"
#include "GL/glew.h"
#include <GL/gl.h>

#include "..\external\stb-master\stb_image.h"

namespace ResourceSystem {

    class TextureResource : public Resource {
    private:
        // Image data
        int width_{ 0 };
        int height_{ 0 };
        int channels_{ 0 };
        unsigned char* imageData_{ nullptr };

        // OpenGL data
        GLuint textureId_{ 0 };
        GLenum format_{ GL_RGB };
        GLenum internalFormat_{ GL_RGB };

        // Determine OpenGL format based on channels
        void DetermineFormat() {
            switch (channels_) {
            case 1:
                format_ = GL_RED;
                internalFormat_ = GL_RED;
                break;
            case 2:
                format_ = GL_RG;
                internalFormat_ = GL_RG;
                break;
            case 3:
                format_ = GL_RGB;
                internalFormat_ = GL_RGB;
                break;
            case 4:
                format_ = GL_RGBA;
                internalFormat_ = GL_RGBA;
                break;
            default:
                format_ = GL_RGB;
                internalFormat_ = GL_RGB;
                break;
            }
        }

    public:
        TextureResource(const std::string& path) : Resource(path) {
            // Set stb_image to flip textures vertically (OpenGL convention)
            stbi_set_flip_vertically_on_load(true);
        }

        ~TextureResource() {
            // Clean up image data if it exists
            if (imageData_) {
                stbi_image_free(imageData_);
                imageData_ = nullptr;
            }

            // Clean up OpenGL texture if it exists
            if (textureId_ != 0) {
                glDeleteTextures(1, &textureId_);
                textureId_ = 0;
            }
        }

        bool Initialize() override {
            if (!Resource::Initialize()) {
                return false;
            }

            // Additional initialization if needed
            return true;
        }

        bool Update(FileSystem::FileSystemManager& fileSystem) override {
            DECLARE_FUNC_LOW();
            if (isLoaded_) {
                return true;
            }

            // Load file data using the file system
            auto result = fileSystem.ReadAllBytes(path_);
            if (!result.IsSuccess()) {
                return false;
            }

            data_ = result.GetValue();

            // Use stb_image to decode the image data
            imageData_ = stbi_load_from_memory(
                data_.data(),
                static_cast<int>(data_.size()),
                &width_,
                &height_,
                &channels_,
                0  // Don't force specific channel count
            );

            if (!imageData_) {
                // Failed to decode image
                return false;
            }

            // Determine OpenGL format
            DetermineFormat();

            isLoaded_ = true;
            return true;
        }

        void Finalize() override {
            DECLARE_FUNC_LOW();
            if (isFinalized_ || !isLoaded_ || !imageData_) {
                Resource::Finalize();
                return;
            }

            // Generate OpenGL texture (this must run on main thread with OpenGL context)
            glGenTextures(1, &textureId_);
            glBindTexture(GL_TEXTURE_2D, textureId_);

            // Upload image data to GPU
            glTexImage2D(
                GL_TEXTURE_2D,
                0,                          // mip level
                internalFormat_,            // internal format
                width_,
                height_,
                0,                          // border (must be 0)
                format_,                    // format
                GL_UNSIGNED_BYTE,           // type
                imageData_                  // data
            );

            // Set default texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            // Generate mipmaps
            glGenerateMipmap(GL_TEXTURE_2D);

            // Unbind texture
            glBindTexture(GL_TEXTURE_2D, 0);

            // Clean up CPU image data (we no longer need it)
            if (imageData_) {
                stbi_image_free(imageData_);
                imageData_ = nullptr;
            }

            // Clear the file data as well to save memory
            data_.clear();
            data_.shrink_to_fit();

            Resource::Finalize();
        }

        // Getters for texture properties
        int GetWidth() const { return width_; }
        int GetHeight() const { return height_; }
        int GetChannels() const { return channels_; }
        GLuint GetTextureId() const { return textureId_; }
        GLenum GetFormat() const { return format_; }
        GLenum GetInternalFormat() const { return internalFormat_; }

        // Utility methods for OpenGL usage
        void Bind(GLenum textureUnit = GL_TEXTURE0) const {
            if (textureId_ != 0 && isFinalized_) {
                glActiveTexture(textureUnit);
                glBindTexture(GL_TEXTURE_2D, textureId_);
            }
        }

        void Unbind() const {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Set texture parameters (call after finalization)
        void SetFilterMode(GLenum minFilter, GLenum magFilter) {
            if (textureId_ != 0 && isFinalized_) {
                glBindTexture(GL_TEXTURE_2D, textureId_);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }

        void SetWrapMode(GLenum wrapS, GLenum wrapT) {
            if (textureId_ != 0 && isFinalized_) {
                glBindTexture(GL_TEXTURE_2D, textureId_);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    };

} // namespace ResourceSystem
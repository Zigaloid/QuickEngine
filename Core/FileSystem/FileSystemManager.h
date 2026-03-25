#pragma once

#include "FileSystemInterface.h"
#include <vector>
#include <memory>
#include <functional>



namespace FileSystem {

    class FileSystemManager : public IFileSystem {
    private:
        std::vector<std::unique_ptr<IFileSystem>> fileSystems_;

        // Helper template to try an operation across all file systems
        template<typename T>
        Result<T> TryOperation(std::function<Result<T>(IFileSystem*)> operation) {
            if (fileSystems_.empty()) 
            {
                const std::string resultString = "No file systems available";
                auto result = Result<T>(resultString);
                return result;
            }

            std::string lastError;
            for (auto& fs : fileSystems_) {
                auto result = operation(fs.get());
                if (result.IsSuccess()) {
                    return result;
                }
                lastError = result.GetError();
            }
            const std::string resultString = "All file systems failed. Last error: " + lastError;
            return Result<T>(resultString);
        }

        // Specialization for void operations
        Result<void> TryVoidOperation(std::function<Result<void>(IFileSystem*)> operation) {
            if (fileSystems_.empty()) {
                return Result<void>("No file systems available");
            }

            std::string lastError;
            for (auto& fs : fileSystems_) {
                auto result = operation(fs.get());
                if (result.IsSuccess()) {
                    return result;
                }
                lastError = result.GetError();
            }

            return Result<void>("All file systems failed. Last error: " + lastError);
        }

        // Helper for boolean operations (try until one returns true, or all return false)
        bool TryBooleanOperation(std::function<bool(IFileSystem*)> operation) {
            for (auto& fs : fileSystems_) {
                if (operation(fs.get())) {
                    return true;
                }
            }
            return false;
        }

    public:
        FileSystemManager() = default;
        ~FileSystemManager() = default;

        // Stack management operations
        void PushFileSystem(std::unique_ptr<IFileSystem> fileSystem) {
            if (fileSystem) {
                fileSystems_.insert(fileSystems_.begin(), std::move(fileSystem));
            }
        }

        void AddFileSystem(std::unique_ptr<IFileSystem> fileSystem) {
            if (fileSystem) {
                fileSystems_.push_back(std::move(fileSystem));
            }
        }

        std::unique_ptr<IFileSystem> PopFileSystem() {
            if (!fileSystems_.empty()) {
                auto fs = std::move(fileSystems_.front());
                fileSystems_.erase(fileSystems_.begin());
                return fs;
            }
            return nullptr;
        }

        std::unique_ptr<IFileSystem> RemoveFileSystem() {
            if (!fileSystems_.empty()) {
                auto fs = std::move(fileSystems_.back());
                fileSystems_.pop_back();
                return fs;
            }
            return nullptr;
        }

        void ClearFileSystems() {
            fileSystems_.clear();
        }

        size_t GetFileSystemCount() const {
            return fileSystems_.size();
        }

        bool IsEmpty() const {
            return fileSystems_.empty();
        }

        // File operations
        Result<std::unique_ptr<IFile>> OpenFile(const std::string& path, FileMode mode) override {
            return TryOperation<std::unique_ptr<IFile>>([&](IFileSystem* fs) {
                return fs->OpenFile(path, mode);
                });
        }

        Result<std::unique_ptr<IFile>> CreateFile(const std::string& path) override {
            return TryOperation<std::unique_ptr<IFile>>([&](IFileSystem* fs) {
                return fs->CreateFile(path);
                });
        }

        Result<void> DeleteFile(const std::string& path) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->DeleteFile(path);
                });
        }

        Result<void> CopyFile(const std::string& sourcePath, const std::string& destinationPath, bool overwrite = false) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->CopyFile(sourcePath, destinationPath, overwrite);
                });
        }

        Result<void> MoveFile(const std::string& sourcePath, const std::string& destinationPath) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->MoveFile(sourcePath, destinationPath);
                });
        }

        // Directory operations
        Result<std::unique_ptr<IDirectory>> OpenDirectory(const std::string& path) override {
            return TryOperation<std::unique_ptr<IDirectory>>([&](IFileSystem* fs) {
                return fs->OpenDirectory(path);
                });
        }

        Result<void> CreateDirectory(const std::string& path) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->CreateDirectory(path);
                });
        }

        Result<void> DeleteDirectory(const std::string& path, bool recursive = false) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->DeleteDirectory(path, recursive);
                });
        }

        Result<void> CopyDirectory(const std::string& sourcePath, const std::string& destinationPath, bool recursive = true) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->CopyDirectory(sourcePath, destinationPath, recursive);
                });
        }

        Result<void> MoveDirectory(const std::string& sourcePath, const std::string& destinationPath) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->MoveDirectory(sourcePath, destinationPath);
                });
        }

        // Path operations
        bool Exists(const std::string& path) override {
            return TryBooleanOperation([&](IFileSystem* fs) {
                return fs->Exists(path);
                });
        }

        bool IsFile(const std::string& path) override {
            return TryBooleanOperation([&](IFileSystem* fs) {
                return fs->IsFile(path);
                });
        }

        bool IsDirectory(const std::string& path) override {
            return TryBooleanOperation([&](IFileSystem* fs) {
                return fs->IsDirectory(path);
                });
        }

        Result<FileInfo> GetInfo(const std::string& path) override {
            return TryOperation<FileInfo>([&](IFileSystem* fs) {
                return fs->GetInfo(path);
                });
        }

        // Path manipulation - these typically don't fail, so we use the first available filesystem
        std::string GetFileName(const std::string& path) override {
            if (!fileSystems_.empty()) {
                return fileSystems_[0]->GetFileName(path);
            }
            return "";
        }

        std::string GetFileNameWithoutExtension(const std::string& path) override {
            if (!fileSystems_.empty()) {
                return fileSystems_[0]->GetFileNameWithoutExtension(path);
            }
            return "";
        }

        std::string GetExtension(const std::string& path) override {
            if (!fileSystems_.empty()) {
                return fileSystems_[0]->GetExtension(path);
            }
            return "";
        }

        std::string GetDirectoryName(const std::string& path) override {
            if (!fileSystems_.empty()) {
                return fileSystems_[0]->GetDirectoryName(path);
            }
            return "";
        }

        std::string GetFullPath(const std::string& path) override {
            if (!fileSystems_.empty()) {
                return fileSystems_[0]->GetFullPath(path);
            }
            return path;
        }

        std::string CombinePath(const std::string& path1, const std::string& path2) override {
            if (!fileSystems_.empty()) {
                return fileSystems_[0]->CombinePath(path1, path2);
            }
            return path1 + "/" + path2; // Fallback
        }

        // Working directory
        Result<std::string> GetCurrentDirectory() override {
            return TryOperation<std::string>([&](IFileSystem* fs) {
                return fs->GetCurrentDirectory();
                });
        }

        Result<void> SetCurrentDirectory(const std::string& path) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->SetCurrentDirectory(path);
                });
        }

        // Special directories
        Result<std::string> GetTempDirectory() override {
            return TryOperation<std::string>([&](IFileSystem* fs) {
                return fs->GetTempDirectory();
                });
        }

        Result<std::string> GetUserDirectory() override {
            return TryOperation<std::string>([&](IFileSystem* fs) {
                return fs->GetUserDirectory();
                });
        }

        Result<std::string> GetApplicationDirectory() override {
            return TryOperation<std::string>([&](IFileSystem* fs) {
                return fs->GetApplicationDirectory();
                });
        }

        // Utility functions
        Result<std::vector<uint8_t>> ReadAllBytes(const std::string& path) override {
            return TryOperation<std::vector<uint8_t>>([&](IFileSystem* fs) {
                return fs->ReadAllBytes(path);
                });
        }

        Result<std::string> ReadAllText(const std::string& path) override {
            return TryOperation<std::string>([&](IFileSystem* fs) {
                return fs->ReadAllText(path);
                });
        }

        Result<void> WriteAllBytes(const std::string& path, const std::vector<uint8_t>& data) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->WriteAllBytes(path, data);
                });
        }

        Result<void> WriteAllText(const std::string& path, const std::string& text) override {
            return TryVoidOperation([&](IFileSystem* fs) {
                return fs->WriteAllText(path, text);
                });
        }
    };

} // namespace FileSystem
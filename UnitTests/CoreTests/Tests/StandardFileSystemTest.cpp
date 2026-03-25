
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

#include "FileSystem/StandardFileSystem.h"

// Forward declaration of factory function
namespace FileSystem {
	std::unique_ptr<IFileSystem> CreateStandardFileSystem();
}

namespace fs = std::filesystem;

class FileSystemTest : public ::testing::Test {
protected:
	void SetUp() override {
		fileSystem_ = FileSystem::CreateStandardFileSystem();

		// Create a temporary test directory
		testDir_ = fs::temp_directory_path() / "filesystem_test";
		fs::create_directories(testDir_);

		// Create subdirectories for testing
		testSubDir_ = testDir_ / "subdir";
		fs::create_directories(testSubDir_);

		// Create some test files
		testFile1_ = testDir_ / "test1.txt";
		testFile2_ = testDir_ / "test2.txt";
		testFile3_ = testSubDir_ / "test3.log";

		std::ofstream(testFile1_) << "Test content 1";
		std::ofstream(testFile2_) << "Test content 2 with more data";
		std::ofstream(testFile3_) << "Log file content";
	}

	void TearDown() override {
		// Clean up test directory
		std::error_code ec;
		fs::remove_all(testDir_, ec);
	}

	std::unique_ptr<FileSystem::IFileSystem> fileSystem_;
	fs::path testDir_;
	fs::path testSubDir_;
	fs::path testFile1_;
	fs::path testFile2_;
	fs::path testFile3_;
};

// Test basic file operations
TEST_F(FileSystemTest, FileExists) {
	EXPECT_TRUE(fileSystem_->Exists(testFile1_.string()));
	EXPECT_TRUE(fileSystem_->Exists(testFile2_.string()));
	EXPECT_FALSE(fileSystem_->Exists((testDir_ / "nonexistent.txt").string()));
}

TEST_F(FileSystemTest, IsFileAndIsDirectory) {
	EXPECT_TRUE(fileSystem_->IsFile(testFile1_.string()));
	EXPECT_FALSE(fileSystem_->IsDirectory(testFile1_.string()));

	EXPECT_TRUE(fileSystem_->IsDirectory(testDir_.string()));
	EXPECT_FALSE(fileSystem_->IsFile(testDir_.string()));

	EXPECT_FALSE(fileSystem_->IsFile((testDir_ / "nonexistent.txt").string()));
	EXPECT_FALSE(fileSystem_->IsDirectory((testDir_ / "nonexistent").string()));
}

TEST_F(FileSystemTest, GetFileInfo) {
	auto result = fileSystem_->GetInfo(testFile1_.string());
	ASSERT_TRUE(result.IsSuccess());

	const auto& info = result.GetValue();
	EXPECT_EQ(info.name, "test1.txt");
	EXPECT_FALSE(info.isDirectory);
	EXPECT_GT(info.size, 0);
	EXPECT_FALSE(info.fullPath.empty());
}

TEST_F(FileSystemTest, GetDirectoryInfo) {
	auto result = fileSystem_->GetInfo(testDir_.string());
	ASSERT_TRUE(result.IsSuccess());

	const auto& info = result.GetValue();
	EXPECT_TRUE(info.isDirectory);
	EXPECT_TRUE(FileSystem::HasAttribute(info.attributes, FileSystem::FileAttributes::Directory));
}

// Test file reading and writing
TEST_F(FileSystemTest, ReadAllText) {
	auto result = fileSystem_->ReadAllText(testFile1_.string());
	ASSERT_TRUE(result.IsSuccess());
	EXPECT_EQ(result.GetValue(), "Test content 1");
}

TEST_F(FileSystemTest, WriteAllText) {
	auto newFile = testDir_ / "write_test.txt";
	std::string content = "Hello, World!\nThis is a test.";

	auto writeResult = fileSystem_->WriteAllText(newFile.string(), content);
	ASSERT_TRUE(writeResult.IsSuccess());

	auto readResult = fileSystem_->ReadAllText(newFile.string());
	ASSERT_TRUE(readResult.IsSuccess());
	EXPECT_EQ(readResult.GetValue(), content);
}

TEST_F(FileSystemTest, ReadAllBytes) {
	auto result = fileSystem_->ReadAllBytes(testFile1_.string());
	ASSERT_TRUE(result.IsSuccess());

	const auto& bytes = result.GetValue();
	std::string content(bytes.begin(), bytes.end());
	EXPECT_EQ(content, "Test content 1");
}

TEST_F(FileSystemTest, WriteAllBytes) {
	auto newFile = testDir_ / "write_bytes_test.bin";
	std::vector<uint8_t> data = { 0x48, 0x65, 0x6C, 0x6C, 0x6F }; // "Hello" in ASCII

	auto writeResult = fileSystem_->WriteAllBytes(newFile.string(), data);
	ASSERT_TRUE(writeResult.IsSuccess());

	auto readResult = fileSystem_->ReadAllBytes(newFile.string());
	ASSERT_TRUE(readResult.IsSuccess());
	EXPECT_EQ(readResult.GetValue(), data);
}

// Test file operations
TEST_F(FileSystemTest, CreateFile) {
	auto newFile = testDir_ / "created_file.txt";

	auto result = fileSystem_->CreateFile(newFile.string());
	ASSERT_TRUE(result.IsSuccess());

	auto file = std::move(result.GetValue());
	EXPECT_TRUE(file->IsOpen());
	EXPECT_TRUE(file->CanWrite());

	auto writeResult = file->WriteAllText("Created file content");
	ASSERT_TRUE(writeResult.IsSuccess());

	file->Close();

	EXPECT_TRUE(fileSystem_->Exists(newFile.string()));
}

TEST_F(FileSystemTest, OpenFileForReading) {
	auto result = fileSystem_->OpenFile(testFile1_.string(), FileSystem::FileMode::Read);
	ASSERT_TRUE(result.IsSuccess());

	auto file = std::move(result.GetValue());
	EXPECT_TRUE(file->IsOpen());
	EXPECT_TRUE(file->CanRead());
	EXPECT_FALSE(file->CanWrite());

	auto textResult = file->ReadAllText();
	ASSERT_TRUE(textResult.IsSuccess());
	EXPECT_EQ(textResult.GetValue(), "Test content 1");
}

TEST_F(FileSystemTest, OpenFileForWriting) {
	auto newFile = testDir_ / "write_mode_test.txt";

	auto result = fileSystem_->OpenFile(newFile.string(), FileSystem::FileMode::Write);
	ASSERT_TRUE(result.IsSuccess());

	auto file = std::move(result.GetValue());
	EXPECT_TRUE(file->IsOpen());
	EXPECT_TRUE(file->CanWrite());
	EXPECT_FALSE(file->CanRead());

	auto writeResult = file->WriteAllText("Write mode content");
	ASSERT_TRUE(writeResult.IsSuccess());

	file->Close();

	auto readResult = fileSystem_->ReadAllText(newFile.string());
	ASSERT_TRUE(readResult.IsSuccess());
	EXPECT_EQ(readResult.GetValue(), "Write mode content");
}

TEST_F(FileSystemTest, FileSeekAndPosition) {
	auto result = fileSystem_->OpenFile(testFile2_.string(), FileSystem::FileMode::Read);
	ASSERT_TRUE(result.IsSuccess());

	auto file = std::move(result.GetValue());

	// Test initial position
	auto posResult = file->GetPosition();
	ASSERT_TRUE(posResult.IsSuccess());
	EXPECT_EQ(posResult.GetValue(), 0);

	// Test seek to end
	auto seekResult = file->Seek(0, FileSystem::SeekOrigin::End);
	ASSERT_TRUE(seekResult.IsSuccess());

	auto sizeResult = file->GetSize();
	ASSERT_TRUE(sizeResult.IsSuccess());

	auto endPosResult = file->GetPosition();
	ASSERT_TRUE(endPosResult.IsSuccess());
	EXPECT_EQ(endPosResult.GetValue(), sizeResult.GetValue());

	// Test seek to beginning
	auto beginSeekResult = file->Seek(0, FileSystem::SeekOrigin::Begin);
	ASSERT_TRUE(beginSeekResult.IsSuccess());
	EXPECT_EQ(beginSeekResult.GetValue(), 0);
}

// Test file manipulation
TEST_F(FileSystemTest, CopyFile) {
	auto destFile = testDir_ / "copied_file.txt";

	auto result = fileSystem_->CopyFile(testFile1_.string(), destFile.string());
	ASSERT_TRUE(result.IsSuccess());

	EXPECT_TRUE(fileSystem_->Exists(destFile.string()));

	auto originalContent = fileSystem_->ReadAllText(testFile1_.string());
	auto copiedContent = fileSystem_->ReadAllText(destFile.string());

	ASSERT_TRUE(originalContent.IsSuccess());
	ASSERT_TRUE(copiedContent.IsSuccess());
	EXPECT_EQ(originalContent.GetValue(), copiedContent.GetValue());
}

TEST_F(FileSystemTest, MoveFile) {
	auto sourceFile = testDir_ / "move_source.txt";
	auto destFile = testDir_ / "move_dest.txt";

	// Create source file
	auto writeResult = fileSystem_->WriteAllText(sourceFile.string(), "Move test content");
	ASSERT_TRUE(writeResult.IsSuccess());

	// Move file
	auto moveResult = fileSystem_->MoveFile(sourceFile.string(), destFile.string());
	ASSERT_TRUE(moveResult.IsSuccess());

	EXPECT_FALSE(fileSystem_->Exists(sourceFile.string()));
	EXPECT_TRUE(fileSystem_->Exists(destFile.string()));

	auto content = fileSystem_->ReadAllText(destFile.string());
	ASSERT_TRUE(content.IsSuccess());
	EXPECT_EQ(content.GetValue(), "Move test content");
}

TEST_F(FileSystemTest, DeleteFile) {
	auto fileToDelete = testDir_ / "delete_me.txt";

	// Create file
	auto writeResult = fileSystem_->WriteAllText(fileToDelete.string(), "Delete me");
	ASSERT_TRUE(writeResult.IsSuccess());
	EXPECT_TRUE(fileSystem_->Exists(fileToDelete.string()));

	// Delete file
	auto deleteResult = fileSystem_->DeleteFile(fileToDelete.string());
	ASSERT_TRUE(deleteResult.IsSuccess());

	EXPECT_FALSE(fileSystem_->Exists(fileToDelete.string()));
}

// Test directory operations
TEST_F(FileSystemTest, OpenDirectory) {
	auto result = fileSystem_->OpenDirectory(testDir_.string());
	ASSERT_TRUE(result.IsSuccess());

	auto directory = std::move(result.GetValue());
	EXPECT_EQ(directory->GetPath(), testDir_.string());

	auto infoResult = directory->GetInfo();
	ASSERT_TRUE(infoResult.IsSuccess());
	EXPECT_TRUE(infoResult.GetValue().isDirectory);
}

TEST_F(FileSystemTest, GetDirectoryFiles) {
	auto result = fileSystem_->OpenDirectory(testDir_.string());
	ASSERT_TRUE(result.IsSuccess());

	auto directory = std::move(result.GetValue());
	auto filesResult = directory->GetFiles();
	ASSERT_TRUE(filesResult.IsSuccess());

	const auto& files = filesResult.GetValue();
	EXPECT_GE(files.size(), 2); // At least test1.txt and test2.txt

	// Check that all returned items are files
	for (const auto& file : files) {
		EXPECT_FALSE(file.isDirectory);
	}
}

TEST_F(FileSystemTest, GetDirectoryDirectories) {
	auto result = fileSystem_->OpenDirectory(testDir_.string());
	ASSERT_TRUE(result.IsSuccess());

	auto directory = std::move(result.GetValue());
	auto dirsResult = directory->GetDirectories();
	ASSERT_TRUE(dirsResult.IsSuccess());

	const auto& dirs = dirsResult.GetValue();
	EXPECT_GE(dirs.size(), 1); // At least the subdir

	// Check that all returned items are directories
	for (const auto& dir : dirs) {
		EXPECT_TRUE(dir.isDirectory);
	}
}

TEST_F(FileSystemTest, GetDirectoryFilesWithPattern) {
	auto result = fileSystem_->OpenDirectory(testDir_.string());
	ASSERT_TRUE(result.IsSuccess());

	auto directory = std::move(result.GetValue());
	auto txtFilesResult = directory->GetFiles("*.txt");
	ASSERT_TRUE(txtFilesResult.IsSuccess());

	const auto& txtFiles = txtFilesResult.GetValue();
	for (const auto& file : txtFiles) {
		EXPECT_TRUE(file.name.ends_with(".txt"));
	}
}

TEST_F(FileSystemTest, CreateDirectory) {
	auto newDir = testDir_ / "new_directory";

	auto result = fileSystem_->CreateDirectory(newDir.string());
	ASSERT_TRUE(result.IsSuccess());

	EXPECT_TRUE(fileSystem_->Exists(newDir.string()));
	EXPECT_TRUE(fileSystem_->IsDirectory(newDir.string()));
}

TEST_F(FileSystemTest, CreateNestedDirectory) {
	auto nestedDir = testDir_ / "level1" / "level2" / "level3";

	auto result = fileSystem_->CreateDirectory(nestedDir.string());
	ASSERT_TRUE(result.IsSuccess());

	EXPECT_TRUE(fileSystem_->Exists(nestedDir.string()));
	EXPECT_TRUE(fileSystem_->IsDirectory(nestedDir.string()));
	EXPECT_TRUE(fileSystem_->IsDirectory((testDir_ / "level1").string()));
	EXPECT_TRUE(fileSystem_->IsDirectory((testDir_ / "level1" / "level2").string()));
}

TEST_F(FileSystemTest, CopyDirectory) {
	auto destDir = testDir_ / "copied_subdir";

	auto result = fileSystem_->CopyDirectory(testSubDir_.string(), destDir.string());
	ASSERT_TRUE(result.IsSuccess());

	EXPECT_TRUE(fileSystem_->Exists(destDir.string()));
	EXPECT_TRUE(fileSystem_->IsDirectory(destDir.string()));

	// Check that files were copied
	auto copiedFile = destDir / "test3.log";
	EXPECT_TRUE(fileSystem_->Exists(copiedFile.string()));

	auto originalContent = fileSystem_->ReadAllText(testFile3_.string());
	auto copiedContent = fileSystem_->ReadAllText(copiedFile.string());

	ASSERT_TRUE(originalContent.IsSuccess());
	ASSERT_TRUE(copiedContent.IsSuccess());
	EXPECT_EQ(originalContent.GetValue(), copiedContent.GetValue());
}

TEST_F(FileSystemTest, DeleteDirectory) {
	auto dirToDelete = testDir_ / "delete_this_dir";

	// Create directory with a file
	auto createResult = fileSystem_->CreateDirectory(dirToDelete.string());
	ASSERT_TRUE(createResult.IsSuccess());

	auto fileInDir = dirToDelete / "file.txt";
	auto writeResult = fileSystem_->WriteAllText(fileInDir.string(), "content");
	ASSERT_TRUE(writeResult.IsSuccess());

	// Delete recursively
	auto deleteResult = fileSystem_->DeleteDirectory(dirToDelete.string(), true);
	ASSERT_TRUE(deleteResult.IsSuccess());

	EXPECT_FALSE(fileSystem_->Exists(dirToDelete.string()));
}

// Test path operations
TEST_F(FileSystemTest, PathManipulation) {
	std::string testPath = "/path/to/file.txt";

	EXPECT_EQ(fileSystem_->GetFileName(testPath), "file.txt");
	EXPECT_EQ(fileSystem_->GetFileNameWithoutExtension(testPath), "file");
	EXPECT_EQ(fileSystem_->GetExtension(testPath), ".txt");
	EXPECT_EQ(fileSystem_->GetDirectoryName(testPath), "/path/to");

	auto combined = fileSystem_->CombinePath("/path/to", "file.txt");
	EXPECT_TRUE(combined.find("file.txt") != std::string::npos);
}

TEST_F(FileSystemTest, GetFullPath) {
	auto result = fileSystem_->GetFullPath(".");
	EXPECT_FALSE(result.empty());

	// Full path should be absolute
	EXPECT_TRUE(result[0] == '/' || (result.length() > 1 && result[1] == ':')); // Unix or Windows
}

// Test working directory operations
TEST_F(FileSystemTest, CurrentDirectory) {
	auto getCurrentResult = fileSystem_->GetCurrentDirectory();
	ASSERT_TRUE(getCurrentResult.IsSuccess());

	std::string originalDir = getCurrentResult.GetValue();
	EXPECT_FALSE(originalDir.empty());

	// Change to test directory
	auto setResult = fileSystem_->SetCurrentDirectory(testDir_.string());
	ASSERT_TRUE(setResult.IsSuccess());

	auto getNewResult = fileSystem_->GetCurrentDirectory();
	ASSERT_TRUE(getNewResult.IsSuccess());

	// Restore original directory
	auto restoreResult = fileSystem_->SetCurrentDirectory(originalDir);
	ASSERT_TRUE(restoreResult.IsSuccess());
}

// Test special directories
TEST_F(FileSystemTest, SpecialDirectories) {
	auto tempResult = fileSystem_->GetTempDirectory();
	ASSERT_TRUE(tempResult.IsSuccess());
	EXPECT_FALSE(tempResult.GetValue().empty());
	EXPECT_TRUE(fileSystem_->IsDirectory(tempResult.GetValue()));

	auto userResult = fileSystem_->GetUserDirectory();
	ASSERT_TRUE(userResult.IsSuccess());
	EXPECT_FALSE(userResult.GetValue().empty());

	auto appResult = fileSystem_->GetApplicationDirectory();
	ASSERT_TRUE(appResult.IsSuccess());
	EXPECT_FALSE(appResult.GetValue().empty());
}

// Test error handling
TEST_F(FileSystemTest, ErrorHandling) {
	// Try to read non-existent file
	auto readResult = fileSystem_->ReadAllText("/nonexistent/path/file.txt");
	ASSERT_TRUE(readResult.HasError());
	EXPECT_FALSE(readResult.GetError().empty());

	// Try to open non-existent directory
	auto dirResult = fileSystem_->OpenDirectory("/nonexistent/path");
	ASSERT_TRUE(dirResult.HasError());
	EXPECT_FALSE(dirResult.GetError().empty());

	// Try to delete non-existent file
	auto deleteResult = fileSystem_->DeleteFile("/nonexistent/file.txt");
	ASSERT_TRUE(deleteResult.HasError());
	EXPECT_FALSE(deleteResult.GetError().empty());
}

// Test file attributes
TEST_F(FileSystemTest, FileAttributes) {
	auto result = fileSystem_->GetInfo(testFile1_.string());
	ASSERT_TRUE(result.IsSuccess());

	const auto& info = result.GetValue();
	EXPECT_FALSE(FileSystem::HasAttribute(info.attributes, FileSystem::FileAttributes::Directory));

	auto dirResult = fileSystem_->GetInfo(testDir_.string());
	ASSERT_TRUE(dirResult.IsSuccess());

	const auto& dirInfo = dirResult.GetValue();
	EXPECT_TRUE(FileSystem::HasAttribute(dirInfo.attributes, FileSystem::FileAttributes::Directory));
}


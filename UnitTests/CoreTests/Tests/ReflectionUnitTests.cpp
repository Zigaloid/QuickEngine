#include <gtest/gtest.h>
#include <fstream>
#include <memory>
#include <chrono>
#include <limits>
#include <string>
#include <vector>
#include <unordered_map>
#include "Reflection/Reflection.h"
#include "Reflection/ReflectionError.h"
#include "CoreSystem/CoreSystem.h"
#include "ComponentSystem/ComponentSystem.h"
#include "Math/Vector3f.h"
#include "Math/Vector4f.h"
#include "Math/Matrix4f.h"

// ============================================================================
// Test Object Definitions
// ============================================================================

class CSubObj : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CSubObj, CReflectedBase);
	int m_value = 0;
	float m_number = 0.0f;
	bool m_flag = false;
	std::string m_string;
	Vector3f m_vector3;
	Vector4f m_vector4;
	Matrix4f m_matrix4;
};

class CParent : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CParent, CReflectedBase);
	int m_value = 0;
	float m_number = 0.0f;
	std::string m_string;
	std::unique_ptr<CSubObj> m_subObj;
	CSubObj m_Obj;
	std::vector<std::unique_ptr<CReflectedBase>> m_vectorObjects;
};

class CPrimitiveVectors : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CPrimitiveVectors, CReflectedBase);
	std::vector<int> m_ints;
	std::vector<float> m_floats;
	std::vector<bool> m_bools;
	std::vector<std::string> m_strings;
};

class CStringBoolMapObj : public CReflectedBase
{
public:
	REFL_DECLARE_OBJECT(CStringBoolMapObj, CReflectedBase);
	std::unordered_map<std::string, bool> m_flags;
};

REFL_DEFINE_OBJECT(CSubObj)
	REFL_DEFINE_FLOAT_MEMBER(CSubObj, m_number),
	REFL_DEFINE_INT_MEMBER(CSubObj, m_value),
	REFL_DEFINE_BOOL_MEMBER(CSubObj, m_flag),
	REFL_DEFINE_STRING_MEMBER(CSubObj, m_string),
	REFL_DEFINE_VECTOR3_MEMBER(CSubObj, m_vector3),
	REFL_DEFINE_VECTOR4_MEMBER(CSubObj, m_vector4),
	REFL_DEFINE_MATRIX4_MEMBER(CSubObj, m_matrix4),
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CParent)
	REFL_DEFINE_OBJECT_MEMBER(CParent, m_Obj),
	REFL_DEFINE_STRING_MEMBER(CParent, m_string),
	REFL_DEFINE_FLOAT_MEMBER(CParent, m_number),
	REFL_DEFINE_INT_MEMBER(CParent, m_value),
	REFL_DEFINE_OBJECT_PTR_MEMBER(CParent, m_subObj),
	REFL_DEFINE_OBJECT_PTR_VECTOR_MEMBER(CParent, m_vectorObjects),
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CPrimitiveVectors)
	REFL_DEFINE_INT_VECTOR_MEMBER(CPrimitiveVectors, m_ints),
	REFL_DEFINE_FLOAT_VECTOR_MEMBER(CPrimitiveVectors, m_floats),
	REFL_DEFINE_BOOL_VECTOR_MEMBER(CPrimitiveVectors, m_bools),
	REFL_DEFINE_STRING_VECTOR_MEMBER(CPrimitiveVectors, m_strings),
REFL_DEFINE_END

REFL_DEFINE_OBJECT(CStringBoolMapObj)
	REFL_DEFINE_STRING_BOOL_MAP_MEMBER(CStringBoolMapObj, m_flags),
REFL_DEFINE_END

// ============================================================================
// Test Fixture
// ============================================================================

class ReflectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!Core::CoreSystem::Initialize()) {
            std::cerr << "ReflectionTest: Failed to initialize CoreSystem!" << std::endl;
            m_coreSystemReady = false;
            return;
        }

        if (!Core::CoreSystem::IsInitialized()) {
            FAIL() << "CoreSystem not initialized";
            return;
        }

        auto* fileSystemManager = Core::CoreSystem::GetFileSystemManager();
        if (!fileSystemManager) {
            FAIL() << "FileSystemManager not available";
            return;
        }

        m_coreSystemReady = true;
        CleanupTestFiles();

        if (Reflection::g_errorHandler) {
            auto* defaultHandler = dynamic_cast<Reflection::DefaultErrorHandler*>(Reflection::g_errorHandler.get());
            if (defaultHandler) {
                m_originalThrowOnError = defaultHandler->ShouldThrow(Reflection::ErrorSeverity::Error);
                defaultHandler->ClearErrors();
            }
        }
    }

    void TearDown() override {
        if (m_coreSystemReady) {
            CleanupTestFiles();
            RestoreErrorHandlerSettings();
            Core::CoreSystem::Shutdown();
        }
    }

    bool ShouldRunTest() const {
        return m_coreSystemReady && Core::CoreSystem::IsInitialized() &&
               Core::CoreSystem::GetFileSystemManager() != nullptr;
    }

    void CleanupTestFiles() {
        auto* fs = Core::CoreSystem::GetFileSystemManager();
        if (!fs) return;

        std::vector<std::string> testFiles = {
            "test_object.json",
            "test_object.bin",
            "test_object.binary",
            "test_object.JSON",
            "test_object.BIN",
            "test_object.txt",
            "test_corrupt.json",
            "test_invalid.json",
            "test_safe_object.json",
            "test_safe_object.bin",
            "test_filesys.json",
            "test_bool_object.json",
            "test_component_array.json",
            "test_prim_vec.json",
            "test_prim_vec.bin",
            "test_vector3.json",
            "test_str_bool_map.json",
            "test_str_bool_map.bin",
        };

        for (const auto& filename : testFiles) {
            if (fs->Exists(filename)) {
                auto result = fs->DeleteFile(filename);
                if (result.HasError()) {
                    std::cerr << "Warning: Failed to delete " << filename
                             << ": " << result.GetError() << std::endl;
                }
            }
        }
    }

    void SetNonThrowingErrorHandler() {
        Reflection::SetErrorHandler(std::make_unique<Reflection::DefaultErrorHandler>(false, false));
    }

    void RestoreErrorHandlerSettings() {
        Reflection::SetErrorHandler(std::make_unique<Reflection::DefaultErrorHandler>(m_originalThrowOnError, false));
    }

    size_t GetErrorCount() {
        if (Reflection::g_errorHandler) {
            auto* handler = dynamic_cast<Reflection::DefaultErrorHandler*>(Reflection::g_errorHandler.get());
            if (handler) {
                size_t count = 0;
                for (const auto& error : handler->GetErrors()) {
                    if (error.severity == Reflection::ErrorSeverity::Error ||
                        error.severity == Reflection::ErrorSeverity::Critical) {
                        count++;
                    }
                }
                return count;
            }
        }
        return 0;
    }

    Reflection::ErrorInfo GetLastError() {
        if (Reflection::g_errorHandler) {
            auto* handler = dynamic_cast<Reflection::DefaultErrorHandler*>(Reflection::g_errorHandler.get());
            if (handler && !handler->GetErrors().empty()) {
                for (auto it = handler->GetErrors().rbegin(); it != handler->GetErrors().rend(); ++it) {
                    if (it->severity == Reflection::ErrorSeverity::Error ||
                        it->severity == Reflection::ErrorSeverity::Critical) {
                        return *it;
                    }
                }
                return handler->GetErrors().back();
            }
        }
        return Reflection::ErrorInfo(Reflection::ErrorSeverity::Error, Reflection::ErrorCategory::Unknown, "No errors found");
    }

    void InitializeTestObject(CSubObj& obj) {
        obj.m_number = 123.456f;
        obj.m_value = 789;
        obj.m_flag = true;
        obj.m_string = "Test String";
        obj.m_vector3.set(1.0f, 2.0f, 3.0f);
        obj.m_vector4.set(1.0f, 2.0f, 3.0f, 4.0f);
        auto& data = obj.m_matrix4.GetWriteData();
        for (size_t i = 0; i < 16; ++i) {
            data[i] = static_cast<float>(i + 1);
        }
    }

    void VerifySubObjectsEqual(const CSubObj& expected, const CSubObj& actual) {
        EXPECT_FLOAT_EQ(expected.m_number, actual.m_number);
        EXPECT_EQ(expected.m_value, actual.m_value);
        EXPECT_EQ(expected.m_flag, actual.m_flag);
        EXPECT_EQ(expected.m_string, actual.m_string);

        EXPECT_FLOAT_EQ(expected.m_vector3.getX(), actual.m_vector3.getX());
        EXPECT_FLOAT_EQ(expected.m_vector3.getY(), actual.m_vector3.getY());
        EXPECT_FLOAT_EQ(expected.m_vector3.getZ(), actual.m_vector3.getZ());

        EXPECT_FLOAT_EQ(expected.m_vector4.getX(), actual.m_vector4.getX());
        EXPECT_FLOAT_EQ(expected.m_vector4.getY(), actual.m_vector4.getY());
        EXPECT_FLOAT_EQ(expected.m_vector4.getZ(), actual.m_vector4.getZ());
        EXPECT_FLOAT_EQ(expected.m_vector4.getW(), actual.m_vector4.getW());

        const auto& expectedMat = expected.m_matrix4.GetData();
        const auto& actualMat = actual.m_matrix4.GetData();
        for (size_t i = 0; i < 16; ++i) {
            EXPECT_FLOAT_EQ(expectedMat[i], actualMat[i]);
        }
    }

    void CreateCorruptedFile(const std::string& filename, const std::string& content) {
        auto* fs = Core::CoreSystem::GetFileSystemManager();
        if (!fs) {
            FAIL() << "FileSystemManager not available for file creation";
            return;
        }
        auto result = fs->WriteAllText(filename, content);
        if (result.HasError()) {
            FAIL() << "Failed to create test file: " << result.GetError();
        }
    }

    bool m_coreSystemReady = false;
    bool m_originalThrowOnError = true;
};

#define CHECK_CORE_SYSTEM() \
    if (!ShouldRunTest()) { \
        std::cout << "Skipping test - CoreSystem not available" << std::endl; \
        return; \
    }

// ============================================================================
// Basic Serialization Tests
// ============================================================================

TEST_F(ReflectionTest, BasicSerializationDeserialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    VerifySubObjectsEqual(original, loaded);
}

TEST_F(ReflectionTest, FloatPropertySerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_number = 3.14159f;

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_FLOAT_EQ(original.m_number, loaded.m_number);
}

TEST_F(ReflectionTest, IntPropertySerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_value = 42;

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_EQ(original.m_value, loaded.m_value);
}

TEST_F(ReflectionTest, BoolPropertySerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_flag = true;

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_EQ(original.m_flag, loaded.m_flag);

    // Also test false
    original.m_flag = false;
    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded2;
    ASSERT_TRUE(loaded2.Read("test_object.json"));

    EXPECT_EQ(original.m_flag, loaded2.m_flag);
}

TEST_F(ReflectionTest, StringPropertySerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_string = "Hello, World!";

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_EQ(original.m_string, loaded.m_string);
}

TEST_F(ReflectionTest, Vector3PropertySerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_vector3.set(10.0f, 20.0f, 30.0f);

    ASSERT_TRUE(original.Write("test_vector3.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_vector3.json"));

    EXPECT_FLOAT_EQ(original.m_vector3.getX(), loaded.m_vector3.getX());
    EXPECT_FLOAT_EQ(original.m_vector3.getY(), loaded.m_vector3.getY());
    EXPECT_FLOAT_EQ(original.m_vector3.getZ(), loaded.m_vector3.getZ());
}

TEST_F(ReflectionTest, Vector4PropertySerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_vector4.set(10.0f, 20.0f, 30.0f, 40.0f);

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_FLOAT_EQ(original.m_vector4.getX(), loaded.m_vector4.getX());
    EXPECT_FLOAT_EQ(original.m_vector4.getY(), loaded.m_vector4.getY());
    EXPECT_FLOAT_EQ(original.m_vector4.getZ(), loaded.m_vector4.getZ());
    EXPECT_FLOAT_EQ(original.m_vector4.getW(), loaded.m_vector4.getW());
}

TEST_F(ReflectionTest, Matrix4PropertySerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    auto& data = original.m_matrix4.GetWriteData();
    for (size_t i = 0; i < 16; ++i) {
        data[i] = static_cast<float>(i * 2.5f);
    }

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    const auto& originalData = original.m_matrix4.GetData();
    const auto& loadedData = loaded.m_matrix4.GetData();

    for (size_t i = 0; i < 16; ++i) {
        EXPECT_FLOAT_EQ(originalData[i], loadedData[i]);
    }
}

// ============================================================================
// Primitive Vector Serialization Tests
// ============================================================================

TEST_F(ReflectionTest, IntVectorSerialization) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    original.m_ints = { 1, 2, 3, -42, 0, std::numeric_limits<int>::max() };

    ASSERT_TRUE(original.Write("test_prim_vec.json"));

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.Read("test_prim_vec.json"));

    ASSERT_EQ(original.m_ints.size(), loaded.m_ints.size());
    for (size_t i = 0; i < original.m_ints.size(); ++i) {
        EXPECT_EQ(original.m_ints[i], loaded.m_ints[i]);
    }
}

TEST_F(ReflectionTest, FloatVectorSerialization) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    original.m_floats = { 1.1f, 2.2f, 3.3f, -0.5f, 0.0f };

    ASSERT_TRUE(original.Write("test_prim_vec.json"));

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.Read("test_prim_vec.json"));

    ASSERT_EQ(original.m_floats.size(), loaded.m_floats.size());
    for (size_t i = 0; i < original.m_floats.size(); ++i) {
        EXPECT_FLOAT_EQ(original.m_floats[i], loaded.m_floats[i]);
    }
}

TEST_F(ReflectionTest, BoolVectorSerialization) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    original.m_bools = { true, false, true, true, false };

    ASSERT_TRUE(original.Write("test_prim_vec.json"));

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.Read("test_prim_vec.json"));

    ASSERT_EQ(original.m_bools.size(), loaded.m_bools.size());
    for (size_t i = 0; i < original.m_bools.size(); ++i) {
        EXPECT_EQ(original.m_bools[i], loaded.m_bools[i]);
    }
}

TEST_F(ReflectionTest, StringVectorSerialization) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    original.m_strings = { "alpha", "beta", "gamma", "", "hello world" };

    ASSERT_TRUE(original.Write("test_prim_vec.json"));

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.Read("test_prim_vec.json"));

    ASSERT_EQ(original.m_strings.size(), loaded.m_strings.size());
    for (size_t i = 0; i < original.m_strings.size(); ++i) {
        EXPECT_EQ(original.m_strings[i], loaded.m_strings[i]);
    }
}

TEST_F(ReflectionTest, EmptyPrimitiveVectors) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    // All vectors are empty by default

    ASSERT_TRUE(original.Write("test_prim_vec.json"));

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.Read("test_prim_vec.json"));

    EXPECT_TRUE(loaded.m_ints.empty());
    EXPECT_TRUE(loaded.m_floats.empty());
    EXPECT_TRUE(loaded.m_bools.empty());
    EXPECT_TRUE(loaded.m_strings.empty());
}

TEST_F(ReflectionTest, AllPrimitiveVectorsCombined) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    original.m_ints = { 10, 20, 30 };
    original.m_floats = { 1.5f, 2.5f };
    original.m_bools = { true, false };
    original.m_strings = { "one", "two", "three" };

    ASSERT_TRUE(original.Write("test_prim_vec.json"));

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.Read("test_prim_vec.json"));

    EXPECT_EQ(original.m_ints, loaded.m_ints);
    EXPECT_EQ(original.m_bools, loaded.m_bools);
    EXPECT_EQ(original.m_strings, loaded.m_strings);
    ASSERT_EQ(original.m_floats.size(), loaded.m_floats.size());
    for (size_t i = 0; i < original.m_floats.size(); ++i) {
        EXPECT_FLOAT_EQ(original.m_floats[i], loaded.m_floats[i]);
    }
}

TEST_F(ReflectionTest, PrimitiveVectorsBinarySerialization) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    original.m_ints = { 100, 200 };
    original.m_floats = { 9.9f };
    original.m_bools = { false, true };
    original.m_strings = { "bin_test" };

    ASSERT_TRUE(original.Write("test_prim_vec.bin"));

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.Read("test_prim_vec.bin"));

    EXPECT_EQ(original.m_ints, loaded.m_ints);
    EXPECT_EQ(original.m_bools, loaded.m_bools);
    EXPECT_EQ(original.m_strings, loaded.m_strings);
    ASSERT_EQ(original.m_floats.size(), loaded.m_floats.size());
    for (size_t i = 0; i < original.m_floats.size(); ++i) {
        EXPECT_FLOAT_EQ(original.m_floats[i], loaded.m_floats[i]);
    }
}

// ============================================================================
// String-Bool Map Serialization Tests
// ============================================================================

TEST_F(ReflectionTest, StringBoolMapSerialization) {
    CHECK_CORE_SYSTEM();

    CStringBoolMapObj original;
    original.m_flags = {
        { "enable_shadows", true },
        { "enable_fog", false },
        { "fullscreen", true },
        { "vsync", false },
        { "debug_mode", true },
    };

    ASSERT_TRUE(original.Write("test_str_bool_map.json"));

    CStringBoolMapObj loaded;
    ASSERT_TRUE(loaded.Read("test_str_bool_map.json"));

    ASSERT_EQ(original.m_flags.size(), loaded.m_flags.size());
    for (const auto& [key, val] : original.m_flags) {
        auto it = loaded.m_flags.find(key);
        ASSERT_NE(it, loaded.m_flags.end()) << "Missing key: " << key;
        EXPECT_EQ(val, it->second) << "Mismatch for key: " << key;
    }
}

TEST_F(ReflectionTest, EmptyStringBoolMap) {
    CHECK_CORE_SYSTEM();

    CStringBoolMapObj original;
    // m_flags is empty by default

    ASSERT_TRUE(original.Write("test_str_bool_map.json"));

    CStringBoolMapObj loaded;
    ASSERT_TRUE(loaded.Read("test_str_bool_map.json"));

    EXPECT_TRUE(loaded.m_flags.empty());
}

TEST_F(ReflectionTest, StringBoolMapSingleEntry) {
    CHECK_CORE_SYSTEM();

    CStringBoolMapObj original;
    original.m_flags = { { "only_key", true } };

    ASSERT_TRUE(original.Write("test_str_bool_map.json"));

    CStringBoolMapObj loaded;
    ASSERT_TRUE(loaded.Read("test_str_bool_map.json"));

    ASSERT_EQ(loaded.m_flags.size(), 1u);
    ASSERT_NE(loaded.m_flags.find("only_key"), loaded.m_flags.end());
    EXPECT_TRUE(loaded.m_flags.at("only_key"));
}

TEST_F(ReflectionTest, StringBoolMapAllFalse) {
    CHECK_CORE_SYSTEM();

    CStringBoolMapObj original;
    original.m_flags = {
        { "a", false },
        { "b", false },
        { "c", false },
    };

    ASSERT_TRUE(original.Write("test_str_bool_map.json"));

    CStringBoolMapObj loaded;
    ASSERT_TRUE(loaded.Read("test_str_bool_map.json"));

    ASSERT_EQ(loaded.m_flags.size(), 3u);
    for (const auto& [key, val] : loaded.m_flags) {
        EXPECT_FALSE(val) << "Expected false for key: " << key;
    }
}

TEST_F(ReflectionTest, StringBoolMapBinarySerialization) {
    CHECK_CORE_SYSTEM();

    CStringBoolMapObj original;
    original.m_flags = {
        { "option_a", true },
        { "option_b", false },
        { "option_c", true },
    };

    ASSERT_TRUE(original.Write("test_str_bool_map.bin"));

    CStringBoolMapObj loaded;
    ASSERT_TRUE(loaded.Read("test_str_bool_map.bin"));

    ASSERT_EQ(original.m_flags.size(), loaded.m_flags.size());
    for (const auto& [key, val] : original.m_flags) {
        auto it = loaded.m_flags.find(key);
        ASSERT_NE(it, loaded.m_flags.end()) << "Missing key: " << key;
        EXPECT_EQ(val, it->second) << "Mismatch for key: " << key;
    }
}

TEST_F(ReflectionTest, StringBoolMapReflectionMapIntegrity) {
    CHECK_CORE_SYSTEM();

    CStringBoolMapObj obj;
    auto& reflectionMap = obj.GetReflectionMap();

    EXPECT_EQ(reflectionMap.size(), 1u);

    auto* prop = reflectionMap[0].GetProperty();
    ASSERT_NE(prop, nullptr);
    EXPECT_STREQ(prop->GetTypeAsString(), "map_string_bool");
}

// ============================================================================
// Complex Object Hierarchy Tests
// ============================================================================

TEST_F(ReflectionTest, ComplexObjectSerialization) {
    CHECK_CORE_SYSTEM();

    CParent original;
    original.m_number = 555.0f;
    original.m_value = 666;
    original.m_string = "Parent String";

    InitializeTestObject(original.m_Obj);

    original.m_subObj = std::make_unique<CSubObj>();
    InitializeTestObject(*original.m_subObj);

    auto subObj1 = std::make_unique<CSubObj>();
    auto subObj2 = std::make_unique<CSubObj>();
    InitializeTestObject(*subObj1);
    InitializeTestObject(*subObj2);
    subObj2->m_value = 999;

    original.m_vectorObjects.push_back(std::move(subObj1));
    original.m_vectorObjects.push_back(std::move(subObj2));

    ASSERT_TRUE(original.Write("test_object.json"));

    CParent loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_FLOAT_EQ(original.m_number, loaded.m_number);
    EXPECT_EQ(original.m_value, loaded.m_value);
    EXPECT_EQ(original.m_string, loaded.m_string);

    VerifySubObjectsEqual(original.m_Obj, loaded.m_Obj);

    ASSERT_NE(loaded.m_subObj, nullptr);
    VerifySubObjectsEqual(*original.m_subObj, *loaded.m_subObj);

    ASSERT_EQ(loaded.m_vectorObjects.size(), 2u);

    auto* loadedSub1 = dynamic_cast<CSubObj*>(loaded.m_vectorObjects[0].get());
    auto* loadedSub2 = dynamic_cast<CSubObj*>(loaded.m_vectorObjects[1].get());

    ASSERT_NE(loadedSub1, nullptr);
    ASSERT_NE(loadedSub2, nullptr);

    EXPECT_EQ(loadedSub1->m_value, 789);
    EXPECT_EQ(loadedSub2->m_value, 999);
}

// ============================================================================
// SafeRead / SafeWrite Tests
// ============================================================================

TEST_F(ReflectionTest, SafeReadWriteMethods) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    auto writeResult = original.SafeWrite("test_safe_object.json");
    ASSERT_TRUE(writeResult.IsSuccess());
    EXPECT_TRUE(writeResult.GetValue());

    CSubObj loaded;
    auto readResult = loaded.SafeRead("test_safe_object.json");
    ASSERT_TRUE(readResult.IsSuccess());
    EXPECT_TRUE(readResult.GetValue());

    VerifySubObjectsEqual(original, loaded);
}

TEST_F(ReflectionTest, SafeReadInvalidFilePath) {
    CHECK_CORE_SYSTEM();

    CSubObj obj;
    auto result = obj.SafeRead("///invalid///path///file.json");
    ASSERT_TRUE(result.IsError());

    const auto& error = result.GetError();
    EXPECT_EQ(error.severity, Reflection::ErrorSeverity::Error);
    EXPECT_EQ(error.category, Reflection::ErrorCategory::FileIO);
    EXPECT_NE(error.message.find("Failed to open file"), std::string::npos);
}

TEST_F(ReflectionTest, SafeWriteInvalidFilePath) {
    CHECK_CORE_SYSTEM();

    CSubObj obj;
    InitializeTestObject(obj);

    auto result = obj.SafeWrite("con|aux|prn|nul");
    ASSERT_TRUE(result.IsError());

    const auto& error = result.GetError();
    EXPECT_EQ(error.severity, Reflection::ErrorSeverity::Error);
    EXPECT_EQ(error.category, Reflection::ErrorCategory::FileIO);
}

TEST_F(ReflectionTest, SafeReadEmptyFilename) {
    CHECK_CORE_SYSTEM();

    CSubObj obj;
    auto result = obj.SafeRead("");
    ASSERT_TRUE(result.IsError());

    const auto& error = result.GetError();
    EXPECT_EQ(error.severity, Reflection::ErrorSeverity::Error);
    EXPECT_EQ(error.category, Reflection::ErrorCategory::FileIO);
    EXPECT_NE(error.message.find("Empty filename"), std::string::npos);
}

TEST_F(ReflectionTest, SafeParserSelectionByExtension) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    auto writeResult = original.SafeWrite("test_safe_object.json");
    ASSERT_TRUE(writeResult.IsSuccess());

    CSubObj loadedJson;
    auto readResult = loadedJson.SafeRead("test_safe_object.json");
    ASSERT_TRUE(readResult.IsSuccess());
    VerifySubObjectsEqual(original, loadedJson);

    writeResult = original.SafeWrite("test_safe_object.bin");
    ASSERT_TRUE(writeResult.IsSuccess());

    CSubObj loadedBinary;
    readResult = loadedBinary.SafeRead("test_safe_object.bin");
    ASSERT_TRUE(readResult.IsSuccess());
    VerifySubObjectsEqual(original, loadedBinary);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ReflectionTest, NullFilenameHandling) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CSubObj obj;
    size_t initialErrorCount = GetErrorCount();

    EXPECT_FALSE(obj.Read(nullptr));
    EXPECT_FALSE(obj.Write(nullptr));

    EXPECT_EQ(GetErrorCount(), initialErrorCount + 2);

    auto lastError = GetLastError();
    EXPECT_EQ(lastError.category, Reflection::ErrorCategory::FileIO);
    EXPECT_NE(lastError.message.find("Null filename"), std::string::npos);
}

TEST_F(ReflectionTest, InvalidFilePathHandling) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CSubObj obj;
    size_t initialErrorCount = GetErrorCount();

    EXPECT_FALSE(obj.Write("///invalid///path///file.json"));
    EXPECT_FALSE(obj.Read("non_existent_file.json"));

    EXPECT_GE(GetErrorCount(), initialErrorCount + 2);
}

TEST_F(ReflectionTest, CorruptedFileHandling) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CreateCorruptedFile("test_corrupt.json", "{ invalid json content ][}");

    size_t initialErrorCount = GetErrorCount();

    CSubObj obj;
    EXPECT_FALSE(obj.Read("test_corrupt.json"));

    EXPECT_GT(GetErrorCount(), initialErrorCount);

    auto lastError = GetLastError();
    EXPECT_EQ(lastError.category, Reflection::ErrorCategory::FileIO);
}

TEST_F(ReflectionTest, EmptyFileHandling) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CreateCorruptedFile("test_invalid.json", "");

    size_t initialErrorCount = GetErrorCount();

    CSubObj obj;
    EXPECT_FALSE(obj.Read("test_invalid.json"));

    EXPECT_GT(GetErrorCount(), initialErrorCount);
}

TEST_F(ReflectionTest, CustomErrorHandler) {
    CHECK_CORE_SYSTEM();

    class TestErrorHandler : public Reflection::IErrorHandler {
    public:
        mutable std::vector<Reflection::ErrorInfo> capturedErrors;
        bool shouldThrow = false;

        void HandleError(const Reflection::ErrorInfo& error) override {
            capturedErrors.push_back(error);
            if (shouldThrow) {
                throw Reflection::ReflectionException(error);
            }
        }

        bool ShouldThrow(Reflection::ErrorSeverity severity) const override {
            return shouldThrow && (severity == Reflection::ErrorSeverity::Error ||
                                  severity == Reflection::ErrorSeverity::Critical);
        }
    };

    auto testHandler = std::make_unique<TestErrorHandler>();
    auto* handlerPtr = testHandler.get();

    Reflection::SetErrorHandler(std::move(testHandler));

    CSubObj obj;
    obj.Read("non_existent_file.json");

    EXPECT_FALSE(handlerPtr->capturedErrors.empty());
    EXPECT_EQ(handlerPtr->capturedErrors.back().category, Reflection::ErrorCategory::FileIO);

    handlerPtr->shouldThrow = true;
    handlerPtr->capturedErrors.clear();

    EXPECT_THROW(obj.Read("non_existent_file.json"), Reflection::ReflectionException);
    EXPECT_FALSE(handlerPtr->capturedErrors.empty());
}

TEST_F(ReflectionTest, ErrorCategorization) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CSubObj obj;
    size_t initialCount = GetErrorCount();
    obj.Read("non_existent_file.json");
    EXPECT_GT(GetErrorCount(), initialCount);

    auto lastError = GetLastError();
    EXPECT_EQ(lastError.category, Reflection::ErrorCategory::FileIO);
    EXPECT_NE(lastError.fileName.find("ReflectionBase.cpp"), std::string::npos);
    EXPECT_GT(lastError.lineNumber, 0);
}

TEST_F(ReflectionTest, ErrorSeverityLevels) {
    CHECK_CORE_SYSTEM();

    auto testHandler = std::make_unique<Reflection::DefaultErrorHandler>(false, false);
    auto* handlerPtr = testHandler.get();
    Reflection::SetErrorHandler(std::move(testHandler));

    CSubObj obj;
    obj.Read("non_existent_file.json");

    const auto& errors = handlerPtr->GetErrors();
    EXPECT_FALSE(errors.empty());

    for (const auto& error : errors) {
        EXPECT_FALSE(error.message.empty());
        EXPECT_NE(error.category, Reflection::ErrorCategory::Unknown);
        EXPECT_FALSE(error.ToString().empty());
    }
}

TEST_F(ReflectionTest, ResultExceptionHandling) {
    CHECK_CORE_SYSTEM();

    CSubObj obj;

    auto errorResult = obj.SafeRead("non_existent_file.json");
    ASSERT_TRUE(errorResult.IsError());
    EXPECT_THROW(errorResult.GetValue(), Reflection::ReflectionException);

    InitializeTestObject(obj);
    auto successResult = obj.SafeWrite("test_object.json");
    ASSERT_TRUE(successResult.IsSuccess());
    EXPECT_THROW(successResult.GetError(), std::logic_error);
}

// ============================================================================
// Reflection Map Introspection Tests
// ============================================================================

TEST_F(ReflectionTest, GetRflClassNameFunctionality) {
    CHECK_CORE_SYSTEM();

    CSubObj subObj;
    CParent parent;

    EXPECT_STREQ(subObj.GetRflClassName(), "CSubObj");
    EXPECT_STREQ(parent.GetRflClassName(), "CParent");
}

TEST_F(ReflectionTest, ReflectionMapIntegrity) {
    CHECK_CORE_SYSTEM();

    CSubObj obj;
    auto& reflectionMap = obj.GetReflectionMap();

    // CSubObj has 7 properties: m_number, m_value, m_flag, m_string, m_vector3, m_vector4, m_matrix4
    EXPECT_EQ(reflectionMap.size(), 7u);

    bool foundFloat = false;
    bool foundInt = false;
    bool foundBool = false;
    bool foundString = false;
    bool foundVector3 = false;
    bool foundVector4 = false;
    bool foundMatrix4 = false;

    for (auto& entry : reflectionMap) {
        auto* prop = entry.GetProperty();
        ASSERT_NE(prop, nullptr);

        std::string typeName = prop->GetTypeAsString();

        if (typeName == "float") foundFloat = true;
        else if (typeName == "int") foundInt = true;
        else if (typeName == "bool") foundBool = true;
        else if (typeName == "string") foundString = true;
        else if (typeName == "vector3") foundVector3 = true;
        else if (typeName == "vector4") foundVector4 = true;
        else if (typeName == "CMatrix4") foundMatrix4 = true;
    }

    EXPECT_TRUE(foundFloat);
    EXPECT_TRUE(foundInt);
    EXPECT_TRUE(foundBool);
    EXPECT_TRUE(foundString);
    EXPECT_TRUE(foundVector3);
    EXPECT_TRUE(foundVector4);
    EXPECT_TRUE(foundMatrix4);
}

TEST_F(ReflectionTest, PrimitiveVectorReflectionMapIntegrity) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors obj;
    auto& reflectionMap = obj.GetReflectionMap();

    EXPECT_EQ(reflectionMap.size(), 4u);

    bool foundIntVec = false;
    bool foundFloatVec = false;
    bool foundBoolVec = false;
    bool foundStringVec = false;

    for (auto& entry : reflectionMap) {
        auto* prop = entry.GetProperty();
        ASSERT_NE(prop, nullptr);

        std::string typeName = prop->GetTypeAsString();

        if (typeName == "vector_int") foundIntVec = true;
        else if (typeName == "vector_float") foundFloatVec = true;
        else if (typeName == "vector_bool") foundBoolVec = true;
        else if (typeName == "vector_string") foundStringVec = true;
    }

    EXPECT_TRUE(foundIntVec);
    EXPECT_TRUE(foundFloatVec);
    EXPECT_TRUE(foundBoolVec);
    EXPECT_TRUE(foundStringVec);
}

TEST_F(ReflectionTest, ReflectionMapValidation) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CSubObj obj;
    size_t initialErrorCount = GetErrorCount();

    ASSERT_TRUE(obj.Write("test_object.json"));

    EXPECT_EQ(GetErrorCount(), initialErrorCount);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(ReflectionTest, ExtremeValueHandling) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_number = std::numeric_limits<float>::max();
    original.m_value = std::numeric_limits<int>::max();

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_FLOAT_EQ(original.m_number, loaded.m_number);
    EXPECT_EQ(original.m_value, loaded.m_value);
}

TEST_F(ReflectionTest, SpecialStringCharacters) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_string = "Special chars: \"quotes\", \\backslash, \n newline, \t tab";

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_EQ(original.m_string, loaded.m_string);
}

TEST_F(ReflectionTest, EmptyStringProperty) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_string = "";

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_EQ(original.m_string, loaded.m_string);
    EXPECT_TRUE(loaded.m_string.empty());
}

TEST_F(ReflectionTest, NullUniquePtr) {
    CHECK_CORE_SYSTEM();

    CParent original;
    original.m_value = 42;
    // m_subObj is nullptr by default

    ASSERT_TRUE(original.Write("test_object.json"));

    CParent loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_EQ(loaded.m_value, 42);
    // Depending on serialization, nullptr may or may not roundtrip
}

TEST_F(ReflectionTest, EmptyVectorOfObjects) {
    CHECK_CORE_SYSTEM();

    CParent original;
    original.m_value = 100;
    // m_vectorObjects is empty by default

    ASSERT_TRUE(original.Write("test_object.json"));

    CParent loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));

    EXPECT_EQ(loaded.m_value, 100);
    EXPECT_TRUE(loaded.m_vectorObjects.empty());
}

// ============================================================================
// Parser Selection Tests
// ============================================================================

TEST_F(ReflectionTest, ParserSelectionByExtension) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    ASSERT_TRUE(original.Write("test_object.json"));

    CSubObj loadedJson;
    ASSERT_TRUE(loadedJson.Read("test_object.json"));
    VerifySubObjectsEqual(original, loadedJson);

    ASSERT_TRUE(original.Write("test_object.bin"));

    CSubObj loadedBinary;
    ASSERT_TRUE(loadedBinary.Read("test_object.bin"));
    VerifySubObjectsEqual(original, loadedBinary);
}

TEST_F(ReflectionTest, DefaultParserForUnknownExtension) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    ASSERT_TRUE(original.Write("test_object.txt"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.txt"));

    VerifySubObjectsEqual(original, loaded);
}

TEST_F(ReflectionTest, CaseInsensitiveExtensionMatching) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    ASSERT_TRUE(original.Write("test_object.JSON"));
    ASSERT_TRUE(original.Write("test_object.BIN"));

    CSubObj loadedJson, loadedBinary;
    ASSERT_TRUE(loadedJson.Read("test_object.JSON"));
    ASSERT_TRUE(loadedBinary.Read("test_object.BIN"));

    VerifySubObjectsEqual(original, loadedJson);
    VerifySubObjectsEqual(original, loadedBinary);
}

TEST_F(ReflectionTest, BinaryExtensionAlternatives) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    ASSERT_TRUE(original.Write("test_object.binary"));

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_object.binary"));

    VerifySubObjectsEqual(original, loaded);
}

// ============================================================================
// FileSystemManager Integration Tests
// ============================================================================

TEST_F(ReflectionTest, FileSystemManagerIntegration) {
    CHECK_CORE_SYSTEM();

    auto* fs = Core::CoreSystem::GetFileSystemManager();
    ASSERT_NE(fs, nullptr);

    CSubObj original;
    InitializeTestObject(original);

    ASSERT_TRUE(original.Write("test_filesys.json"));

    EXPECT_TRUE(fs->Exists("test_filesys.json"));
    EXPECT_TRUE(fs->IsFile("test_filesys.json"));

    auto textResult = fs->ReadAllText("test_filesys.json");
    ASSERT_TRUE(textResult.IsSuccess());
    EXPECT_FALSE(textResult.GetValue().empty());

    CSubObj loaded;
    ASSERT_TRUE(loaded.Read("test_filesys.json"));
    VerifySubObjectsEqual(original, loaded);

    auto deleteResult = fs->DeleteFile("test_filesys.json");
    EXPECT_TRUE(deleteResult.IsSuccess());
    EXPECT_FALSE(fs->Exists("test_filesys.json"));
}

// ============================================================================
// Performance Test
// ============================================================================

TEST_F(ReflectionTest, LargeObjectPerformanceWithErrorHandling) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CParent original;
    original.m_number = 555.0f;
    original.m_value = 666;
    original.m_string = "Parent String";
    InitializeTestObject(original.m_Obj);

    original.m_subObj = std::make_unique<CSubObj>();
    InitializeTestObject(*original.m_subObj);

    const int numObjects = 10;

    for (int i = 0; i < numObjects; ++i) {
        auto subObj = std::make_unique<CSubObj>();
        InitializeTestObject(*subObj);
        subObj->m_value = i;
        subObj->m_number = static_cast<float>(i) * 1.5f;
        subObj->m_string = "Object " + std::to_string(i);
        original.m_vectorObjects.push_back(std::move(subObj));
    }

    size_t initialErrorCount = GetErrorCount();

    auto start = std::chrono::high_resolution_clock::now();
    ASSERT_TRUE(original.Write("test_object.json"));
    auto writeEnd = std::chrono::high_resolution_clock::now();

    CParent loaded;
    ASSERT_TRUE(loaded.Read("test_object.json"));
    auto readEnd = std::chrono::high_resolution_clock::now();

    auto writeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(writeEnd - start);
    auto readDuration = std::chrono::duration_cast<std::chrono::milliseconds>(readEnd - writeEnd);

    EXPECT_LT(writeDuration.count(), 1000);
    EXPECT_LT(readDuration.count(), 1000);

    EXPECT_EQ(GetErrorCount(), initialErrorCount);

    EXPECT_EQ(loaded.m_vectorObjects.size(), static_cast<size_t>(numObjects));

    ASSERT_NE(loaded.m_subObj, nullptr);
    VerifySubObjectsEqual(*original.m_subObj, *loaded.m_subObj);

    for (int i = 0; i < numObjects && i < static_cast<int>(loaded.m_vectorObjects.size()); ++i) {
        auto* loadedSub = dynamic_cast<CSubObj*>(loaded.m_vectorObjects[i].get());
        ASSERT_NE(loadedSub, nullptr);
        EXPECT_EQ(loadedSub->m_value, i);
        EXPECT_FLOAT_EQ(loadedSub->m_number, static_cast<float>(i) * 1.5f);
        EXPECT_EQ(loadedSub->m_string, "Object " + std::to_string(i));
    }
}

// ============================================================================
// Component System Integration Tests
// ============================================================================

using namespace ComponentSystem;

class TestArrayComponent : public Component {
public:
	REFL_DECLARE_OBJECT(TestArrayComponent, Component);
	int m_value = 0;
};

REFL_DEFINE_OBJECT(TestArrayComponent)
    REFL_DEFINE_INT_MEMBER(TestArrayComponent, m_value),
REFL_DEFINE_END

class CComponentArrayObj : public CReflectedBase {
public:
	REFL_DECLARE_OBJECT(CComponentArrayObj, CReflectedBase);
	std::vector<TestArrayComponent*> m_componentArray;
};

REFL_DEFINE_OBJECT(CComponentArrayObj)
    REFL_DEFINE_COMPONENT_PTR_VECTOR_MEMBER(CComponentArrayObj, m_componentArray),
REFL_DEFINE_END

TEST_F(ReflectionTest, ComponentPtrArrayPropertySerialization) {
    CHECK_CORE_SYSTEM();

    ComponentSystem::ComponentManager* componentManager = Core::CoreSystem::GetComponentManager();
    ASSERT_NE(componentManager, nullptr);
    componentManager->RegisterComponentType<TestArrayComponent>(TestArrayComponent::ClassName());

    TestArrayComponent* comp1 = new TestArrayComponent();
    TestArrayComponent* comp2 = new TestArrayComponent();

    comp1->m_value = 1;
    comp2->m_value = 2;

    CComponentArrayObj original;
    original.m_componentArray.push_back(comp1);
    original.m_componentArray.push_back(comp2);

    ASSERT_TRUE(original.Write("test_component_array.json"));

    CComponentArrayObj loaded;
    ASSERT_TRUE(loaded.Read("test_component_array.json"));

    ASSERT_EQ(loaded.m_componentArray.size(), 2u);
    EXPECT_EQ(loaded.m_componentArray[0]->m_value, 1);
    EXPECT_EQ(loaded.m_componentArray[1]->m_value, 2);

    delete comp1;
    delete comp2;
}

// ============================================================================
// In-Memory JSON String Serialization Tests
// ============================================================================

TEST_F(ReflectionTest, WriteToJsonStringBasic) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    auto result = original.WriteToJsonString();
    ASSERT_TRUE(result.IsSuccess());

    const std::string& jsonString = result.GetValue();
    EXPECT_FALSE(jsonString.empty());
    // Sanity: the output should contain some expected member names
    EXPECT_NE(jsonString.find("m_number"), std::string::npos);
    EXPECT_NE(jsonString.find("m_value"), std::string::npos);
    EXPECT_NE(jsonString.find("m_string"), std::string::npos);
}

TEST_F(ReflectionTest, ReadFromJsonStringBasic) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    auto writeResult = original.WriteToJsonString();
    ASSERT_TRUE(writeResult.IsSuccess());

    CSubObj loaded;
    ASSERT_TRUE(loaded.ReadFromJsonString(writeResult.GetValue()));

    VerifySubObjectsEqual(original, loaded);
}

TEST_F(ReflectionTest, JsonStringRoundTripComplexObject) {
    CHECK_CORE_SYSTEM();

    CParent original;
    original.m_number = 555.0f;
    original.m_value = 666;
    original.m_string = "Parent String";

    InitializeTestObject(original.m_Obj);

    original.m_subObj = std::make_unique<CSubObj>();
    InitializeTestObject(*original.m_subObj);

    auto subObj1 = std::make_unique<CSubObj>();
    auto subObj2 = std::make_unique<CSubObj>();
    InitializeTestObject(*subObj1);
    InitializeTestObject(*subObj2);
    subObj2->m_value = 999;

    original.m_vectorObjects.push_back(std::move(subObj1));
    original.m_vectorObjects.push_back(std::move(subObj2));

    auto writeResult = original.WriteToJsonString();
    ASSERT_TRUE(writeResult.IsSuccess());

    CParent loaded;
    ASSERT_TRUE(loaded.ReadFromJsonString(writeResult.GetValue()));

    EXPECT_FLOAT_EQ(original.m_number, loaded.m_number);
    EXPECT_EQ(original.m_value, loaded.m_value);
    EXPECT_EQ(original.m_string, loaded.m_string);

    VerifySubObjectsEqual(original.m_Obj, loaded.m_Obj);

    ASSERT_NE(loaded.m_subObj, nullptr);
    VerifySubObjectsEqual(*original.m_subObj, *loaded.m_subObj);

    ASSERT_EQ(loaded.m_vectorObjects.size(), 2u);

    auto* loadedSub1 = dynamic_cast<CSubObj*>(loaded.m_vectorObjects[0].get());
    auto* loadedSub2 = dynamic_cast<CSubObj*>(loaded.m_vectorObjects[1].get());

    ASSERT_NE(loadedSub1, nullptr);
    ASSERT_NE(loadedSub2, nullptr);

    EXPECT_EQ(loadedSub1->m_value, 789);
    EXPECT_EQ(loadedSub2->m_value, 999);
}

TEST_F(ReflectionTest, JsonStringRoundTripPrimitiveVectors) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    original.m_ints = { 10, 20, 30 };
    original.m_floats = { 1.5f, 2.5f };
    original.m_bools = { true, false };
    original.m_strings = { "one", "two", "three" };

    auto writeResult = original.WriteToJsonString();
    ASSERT_TRUE(writeResult.IsSuccess());

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.ReadFromJsonString(writeResult.GetValue()));

    EXPECT_EQ(original.m_ints, loaded.m_ints);
    EXPECT_EQ(original.m_bools, loaded.m_bools);
    EXPECT_EQ(original.m_strings, loaded.m_strings);
    ASSERT_EQ(original.m_floats.size(), loaded.m_floats.size());
    for (size_t i = 0; i < original.m_floats.size(); ++i) {
        EXPECT_FLOAT_EQ(original.m_floats[i], loaded.m_floats[i]);
    }
}

TEST_F(ReflectionTest, JsonStringRoundTripStringBoolMap) {
    CHECK_CORE_SYSTEM();

    CStringBoolMapObj original;
    original.m_flags = {
        { "enable_shadows", true },
        { "enable_fog", false },
        { "fullscreen", true },
    };

    auto writeResult = original.WriteToJsonString();
    ASSERT_TRUE(writeResult.IsSuccess());

    CStringBoolMapObj loaded;
    ASSERT_TRUE(loaded.ReadFromJsonString(writeResult.GetValue()));

    ASSERT_EQ(original.m_flags.size(), loaded.m_flags.size());
    for (const auto& [key, val] : original.m_flags) {
        auto it = loaded.m_flags.find(key);
        ASSERT_NE(it, loaded.m_flags.end()) << "Missing key: " << key;
        EXPECT_EQ(val, it->second) << "Mismatch for key: " << key;
    }
}

TEST_F(ReflectionTest, ReadFromJsonStringEmptyInput) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CSubObj obj;
    EXPECT_FALSE(obj.ReadFromJsonString(""));
}

TEST_F(ReflectionTest, ReadFromJsonStringInvalidJson) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CSubObj obj;
    EXPECT_FALSE(obj.ReadFromJsonString("{ this is not valid json ][}"));
}

TEST_F(ReflectionTest, WriteToJsonStringMatchesFileSerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    // Write to file
    ASSERT_TRUE(original.Write("test_object.json"));

    // Read back from file
    CSubObj fromFile;
    ASSERT_TRUE(fromFile.Read("test_object.json"));

    // Write to JSON string and read back
    auto writeResult = original.WriteToJsonString();
    ASSERT_TRUE(writeResult.IsSuccess());

    CSubObj fromString;
    ASSERT_TRUE(fromString.ReadFromJsonString(writeResult.GetValue()));

    // Both should produce identical objects
    VerifySubObjectsEqual(fromFile, fromString);
}

// ============================================================================
// In-Memory Binary Buffer Serialization Tests
// ============================================================================

TEST_F(ReflectionTest, WriteToBinaryBufferBasic) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    auto result = original.WriteToBinaryBuffer();
    ASSERT_TRUE(result.IsSuccess());

    const std::vector<uint8_t>& binaryData = result.GetValue();
    EXPECT_FALSE(binaryData.empty());
}

TEST_F(ReflectionTest, ReadFromBinaryBufferBasic) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CSubObj loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    VerifySubObjectsEqual(original, loaded);
}

TEST_F(ReflectionTest, BinaryBufferRoundTripComplexObject) {
    CHECK_CORE_SYSTEM();

    CParent original;
    original.m_number = 555.0f;
    original.m_value = 666;
    original.m_string = "Parent String";

    InitializeTestObject(original.m_Obj);

    original.m_subObj = std::make_unique<CSubObj>();
    InitializeTestObject(*original.m_subObj);

    auto subObj1 = std::make_unique<CSubObj>();
    auto subObj2 = std::make_unique<CSubObj>();
    InitializeTestObject(*subObj1);
    InitializeTestObject(*subObj2);
    subObj2->m_value = 999;

    original.m_vectorObjects.push_back(std::move(subObj1));
    original.m_vectorObjects.push_back(std::move(subObj2));

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CParent loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    EXPECT_FLOAT_EQ(original.m_number, loaded.m_number);
    EXPECT_EQ(original.m_value, loaded.m_value);
    EXPECT_EQ(original.m_string, loaded.m_string);

    VerifySubObjectsEqual(original.m_Obj, loaded.m_Obj);

    ASSERT_NE(loaded.m_subObj, nullptr);
    VerifySubObjectsEqual(*original.m_subObj, *loaded.m_subObj);

    ASSERT_EQ(loaded.m_vectorObjects.size(), 2u);

    auto* loadedSub1 = dynamic_cast<CSubObj*>(loaded.m_vectorObjects[0].get());
    auto* loadedSub2 = dynamic_cast<CSubObj*>(loaded.m_vectorObjects[1].get());

    ASSERT_NE(loadedSub1, nullptr);
    ASSERT_NE(loadedSub2, nullptr);

    EXPECT_EQ(loadedSub1->m_value, 789);
    EXPECT_EQ(loadedSub2->m_value, 999);
}

TEST_F(ReflectionTest, BinaryBufferRoundTripPrimitiveVectors) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    original.m_ints = { 10, 20, 30 };
    original.m_floats = { 1.5f, 2.5f };
    original.m_bools = { true, false };
    original.m_strings = { "one", "two", "three" };

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    EXPECT_EQ(original.m_ints, loaded.m_ints);
    EXPECT_EQ(original.m_bools, loaded.m_bools);
    EXPECT_EQ(original.m_strings, loaded.m_strings);
    ASSERT_EQ(original.m_floats.size(), loaded.m_floats.size());
    for (size_t i = 0; i < original.m_floats.size(); ++i) {
        EXPECT_FLOAT_EQ(original.m_floats[i], loaded.m_floats[i]);
    }
}

TEST_F(ReflectionTest, BinaryBufferRoundTripStringBoolMap) {
    CHECK_CORE_SYSTEM();

    CStringBoolMapObj original;
    original.m_flags = {
        { "enable_shadows", true },
        { "enable_fog", false },
        { "fullscreen", true },
    };

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CStringBoolMapObj loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    ASSERT_EQ(original.m_flags.size(), loaded.m_flags.size());
    for (const auto& [key, val] : original.m_flags) {
        auto it = loaded.m_flags.find(key);
        ASSERT_NE(it, loaded.m_flags.end()) << "Missing key: " << key;
        EXPECT_EQ(val, it->second) << "Mismatch for key: " << key;
    }
}

TEST_F(ReflectionTest, ReadFromBinaryBufferEmptyInput) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CSubObj obj;
    std::vector<uint8_t> emptyBuffer;
    EXPECT_FALSE(obj.ReadFromBinaryBuffer(emptyBuffer));
}

TEST_F(ReflectionTest, ReadFromBinaryBufferCorruptData) {
    CHECK_CORE_SYSTEM();

    SetNonThrowingErrorHandler();

    CSubObj obj;
    std::vector<uint8_t> garbage = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF };
    EXPECT_FALSE(obj.ReadFromBinaryBuffer(garbage));
}

TEST_F(ReflectionTest, BinaryBufferMatchesFileSerialization) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    InitializeTestObject(original);

    // Write to .bin file and read back
    ASSERT_TRUE(original.Write("test_object.bin"));

    CSubObj fromFile;
    ASSERT_TRUE(fromFile.Read("test_object.bin"));

    // Write to binary buffer and read back
    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CSubObj fromBuffer;
    ASSERT_TRUE(fromBuffer.ReadFromBinaryBuffer(writeResult.GetValue()));

    // Both should produce identical objects
    VerifySubObjectsEqual(fromFile, fromBuffer);
}

TEST_F(ReflectionTest, BinaryBufferEmptyPrimitiveVectors) {
    CHECK_CORE_SYSTEM();

    CPrimitiveVectors original;
    // All vectors are empty by default

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CPrimitiveVectors loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    EXPECT_TRUE(loaded.m_ints.empty());
    EXPECT_TRUE(loaded.m_floats.empty());
    EXPECT_TRUE(loaded.m_bools.empty());
    EXPECT_TRUE(loaded.m_strings.empty());
}

TEST_F(ReflectionTest, BinaryBufferExtremeValues) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_number = std::numeric_limits<float>::max();
    original.m_value = std::numeric_limits<int>::max();
    original.m_flag = true;
    original.m_string = "Extreme values test";

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CSubObj loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    EXPECT_FLOAT_EQ(original.m_number, loaded.m_number);
    EXPECT_EQ(original.m_value, loaded.m_value);
    EXPECT_EQ(original.m_flag, loaded.m_flag);
    EXPECT_EQ(original.m_string, loaded.m_string);
}

TEST_F(ReflectionTest, BinaryBufferSpecialStringCharacters) {
    CHECK_CORE_SYSTEM();

    CSubObj original;
    original.m_string = "Special chars: \"quotes\", \\backslash, \n newline, \t tab";

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CSubObj loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    EXPECT_EQ(original.m_string, loaded.m_string);
}

TEST_F(ReflectionTest, BinaryBufferNullUniquePtr) {
    CHECK_CORE_SYSTEM();

    CParent original;
    original.m_value = 42;
    // m_subObj is nullptr by default

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CParent loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    EXPECT_EQ(loaded.m_value, 42);
}

TEST_F(ReflectionTest, BinaryBufferEmptyVectorOfObjects) {
    CHECK_CORE_SYSTEM();

    CParent original;
    original.m_value = 100;
    // m_vectorObjects is empty by default

    auto writeResult = original.WriteToBinaryBuffer();
    ASSERT_TRUE(writeResult.IsSuccess());

    CParent loaded;
    ASSERT_TRUE(loaded.ReadFromBinaryBuffer(writeResult.GetValue()));

    EXPECT_EQ(loaded.m_value, 100);
    EXPECT_TRUE(loaded.m_vectorObjects.empty());
}
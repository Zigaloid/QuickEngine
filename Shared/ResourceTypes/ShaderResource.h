#pragma once
#include "Reflection/ReflectionBase.h"
#include "ResourceManager/ResourceManager.h"
#include "bgfx\bgfx.h"
#include "bgfx_utils.h"

#include "CoreSystem\CoreSystem.h"

class CShaderResource : public ResourceSystem::Resource
{
public:
	explicit CShaderResource(const std::string& path)
		: Resource(path)
		, m_shader(BGFX_INVALID_HANDLE)
	{
	}
	// path is used as the base program name.
	// Convention: "mesh" will load shaders "vs_mesh" and "fs_mesh".
	bgfx::ShaderHandle GetShaderHandle() const { return m_shader; }

	~CShaderResource() override
	{
		if (bgfx::isValid(m_shader))
		{
			bgfx::destroy(m_shader);
			m_shader = BGFX_INVALID_HANDLE;
		}
	}

	// Finalize runs on the main thread – safe for bgfx resource creation.
	// Constructs vertex/fragment shader names from the base name:
	//   "mesh" -> loadProgram("vs_mesh", "fs_mesh")
	void Finalize() override
	{
		m_shader = loadShaderFromMemory(GetData().data(), static_cast<uint32_t>(GetLoadedSize()), m_path.c_str());
		m_isFinalized = bgfx::isValid(m_shader);
	}
private:
	bgfx::ShaderHandle m_shader = BGFX_INVALID_HANDLE;
};

class CShaderResourceReference : public CTypedResourceReference<CShaderResource>
{
public:
	REFL_DECLARE_OBJECT(CShaderResourceReference, CResourceReference);
};
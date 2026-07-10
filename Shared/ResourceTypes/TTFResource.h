#pragma once
#include "ResourceManager/ResourceManager.h"
#include "../bgfx_common/font/font_manager.h"

class CTTFResource : public ResourceSystem::Resource
{
public:
	static std::vector<std::string_view> GetSupportedExtensions()
	{
		return { ".ttf" };
	}

	explicit CTTFResource(const std::string& path)
		: Resource(path)
		, m_trueTypeHandle({ UINT16_MAX })
	{
	}

	// ── Two-phase loading ────────────────────────────────────────────────

	bool Update(FileSystem::FileSystemManager& fileSystem) override
	{
		return Resource::Update(fileSystem);
	}

	void Finalize() override;

	// ── Accessor ─────────────────────────────────────────────────────────

	TrueTypeHandle GetTrueTypeHandle() const { return m_trueTypeHandle; }

private:
	TrueTypeHandle m_trueTypeHandle;
};

class CTTFResourceReference : public CTypedResourceReference<CTTFResource>
{
public:
	REFL_DECLARE_OBJECT(CTTFResourceReference, CResourceReference);
};

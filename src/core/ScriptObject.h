#pragma once
#include <string>

#include "Object.h"

class Script;

class ScriptObject : public Object
{
public:
	static ScriptObject* Create(const ObjectCreationData& data);
	static ScriptObject* Create();

	void Start() override;
	void Update(float delta) override;

	~ScriptObject() override;

	void SetScript(const std::string& file);

	void Reload();

private:
	ScriptObject();

	void Init(const ObjectCreationData& data);

	Script* pScript = nullptr;
	std::string sourceFile;
};
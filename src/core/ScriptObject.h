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

	void SetScriptFile(const std::string& file);
	void SetScriptCode(const std::string& code);

	void Reload();

	bool pause = false;

private:
	ScriptObject();

	void Init(const ObjectCreationData& data);

	void InitializeScript(); // does NOT call pScript->Start()

	void DuplicateDataTo(Object* pObject) const override;

	void SerializeSelf(BinaryStream& stream) const override;
	void DeserializeSelf(const BinarySpan& stream) override;

	Script* pScript = nullptr;
	std::string sourceCode;
};
#include "core/ScriptObject.h"

#include "scripting/Script.h"

ScriptObject::ScriptObject() : Object(Object::InheritType::Script)
{

}

ScriptObject* ScriptObject::Create(const ObjectCreationData& data)
{
	ScriptObject* pObject = new ScriptObject();
	pObject->Init(data);

	return pObject;
}

ScriptObject* ScriptObject::Create()
{
	return new ScriptObject();
}

void ScriptObject::Init(const ObjectCreationData& data)
{
	Initialize(data);
}

void ScriptObject::Start()
{
	if (pScript == nullptr)
		return;

	pScript->Start();
}

void ScriptObject::Update(float delta)
{
	if (pScript == nullptr || pause)
		return;

	pScript->Update(delta);
}

ScriptObject::~ScriptObject()
{
	if (pScript == nullptr)
		return;

	pScript->Destroy();
	delete pScript;
}

void ScriptObject::SetScript(const std::string& file)
{
	if (sourceFile != file)
		sourceFile = file;

	if (pScript != nullptr)
	{
		pScript->Reset(file);
	}
	else
	{
		pScript = new Script(file, this);
	}
	pScript->Start();
	hasScript = true;
}

void ScriptObject::Reload()
{
	SetScript(sourceFile);
}
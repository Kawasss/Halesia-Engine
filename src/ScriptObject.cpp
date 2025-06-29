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

	pScript->SetTransform(transform);
	pScript->Start();
	pScript->GetTransform(transform);
}

void ScriptObject::Update(float delta)
{
	if (pScript == nullptr || pause)
		return;

	pScript->SetTransform(transform);
	pScript->Update(delta);
	pScript->GetTransform(transform);
}

ScriptObject::~ScriptObject()
{
	if (pScript == nullptr)
		return;

	pScript->SetTransform(transform);
	pScript->Destroy();
	pScript->GetTransform(transform);
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
		pScript = new Script(file);
	}
	pScript->SetTransform(transform);
	pScript->Start();
	pScript->GetTransform(transform);
	hasScript = true;
}

void ScriptObject::Reload()
{
	SetScript(sourceFile);
}
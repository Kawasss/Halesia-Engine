module Core.ScriptObject;

import std;

import Scripting.Script;

import IO.BinaryStream;
import IO;

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
}

void ScriptObject::SetScriptFile(const std::string& file)
{
	std::expected<std::vector<char>, bool> code = IO::ReadFile(file, IO::ReadOptions::AddNullTerminator);
	if (!code.has_value())
		return;

	sourceCode = reinterpret_cast<const char*>(code->data());
	if (sourceCode.empty())
		return;

	InitializeScript();
	pScript->Start();
	hasScript = true;
}

void ScriptObject::SetScriptCode(const std::string& code)
{
	sourceCode = code;
	if (sourceCode.empty())
		return;

	InitializeScript();
	pScript->Start();
	hasScript = true;
}

void ScriptObject::InitializeScript()
{
	if (pScript != nullptr)
	{
		pScript->Reset(sourceCode);
	}
	else
	{
		pScript = std::make_unique<Script>(sourceCode, this);
	}
}

void ScriptObject::Reload()
{
	SetScriptCode(sourceCode);
}

void ScriptObject::DuplicateDataTo(Object* pObject) const
{
	if (sourceCode.empty())
		return;

	ScriptObject* pScriptObj = dynamic_cast<ScriptObject*>(pObject);

	pScriptObj->SetScriptCode(sourceCode);
}

void ScriptObject::SerializeSelf(BinaryStream& stream) const
{
	uint32_t strLen = static_cast<uint32_t>(sourceCode.size());

	stream << strLen;

	if (strLen == 0)
		return;

	stream.Write(sourceCode.data(), strLen);
}

void ScriptObject::DeserializeSelf(const BinarySpan& stream)
{
	uint32_t strLen = 0;
	stream >> strLen;

	if (strLen == 0)
		return;

	sourceCode.resize(strLen);
	stream.Read(sourceCode.data(), strLen);

	InitializeScript();
	pScript->Start();
	hasScript = true;
}
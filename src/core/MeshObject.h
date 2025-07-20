#pragma once
#include "Object.h"

#include "../io/FwdDclCreationData.h"

#include "../renderer/Mesh.h"

class MeshObject : public Object
{
public:
	static MeshObject* Create(const ObjectCreationData& data);
	static MeshObject* Create();

	~MeshObject() override;

	bool MeshIsValid() const;

	Mesh mesh;

private:
	MeshObject();

	void Init(const ObjectCreationData& data);

protected:
	void DuplicateDataTo(Object* pObject) const override;

	void SerializeSelf(BinaryStream& stream) const override;
	void DeserializeSelf(const BinarySpan& stream) override;
};
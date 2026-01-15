#pragma once
import Physics.RigidBody;

#include "Object.h"

#include "../io/FwdDclCreationData.h"

class Rigid3DObject : public Object
{
public:
	static Rigid3DObject* Create(const ObjectCreationData& data);
	static Rigid3DObject* Create();

	~Rigid3DObject() override;

	RigidBody rigid;

private:
	Rigid3DObject();
	void Init(const ObjectCreationData& data);

protected:
	void DuplicateDataTo(Object* pObject) const override;
};
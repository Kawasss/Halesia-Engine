#pragma once
#include "Object.h"

#include "../physics/RigidBody.h"

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
};
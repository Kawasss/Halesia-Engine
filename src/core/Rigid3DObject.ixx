export module Core.Rigid3DObject;

import IO.CreationData;

import Core.Object;

import Physics.RigidBody;

export class Rigid3DObject : public Object
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
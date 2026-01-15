module;

#include "Windows.h"

#include "Object.h"
#include "../io/CreationData.h"

#include "../renderer/Light.h"

#include "../glm.h"

#include "../io/BinaryStream.h"

export module Core.LightObject;

export class LightObject : public Object
{
public:
	static LightObject* Create(const ObjectCreationData& data);
	static LightObject* Create(const Light& light);
	static LightObject* Create();

	Light::Type lType;

	float cutoff = .0f;
	float outerCutoff = .0f;

	glm::vec3 color = glm::vec3(1.0f);

	LightGPU ToGPUFormat() const;

private:
	LightObject();
	void Init(const ObjectCreationData& data);

protected:
	void DuplicateDataTo(Object* pObject) const override;

	void SerializeSelf(BinaryStream& stream) const override;
	void DeserializeSelf(const BinarySpan& stream) override;
};
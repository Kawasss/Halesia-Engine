#pragma once
#include <vector>
#include <string>

#include "../glm.h"

struct aiNodeAnim;

struct KeyPosition
{
    glm::vec3 position;
    float timeStamp;
};

struct KeyRotation
{
    glm::quat orientation;
    float timeStamp;
};

struct KeyScale
{
    glm::vec3 scale;
    float timeStamp;
};

struct BoneInfo
{
    int index;
    glm::mat4 offset;
};

class Bone
{
public:
    Bone(const aiNodeAnim* animNode);
    void Update(float time);

    glm::mat4 GetTransform() const;
    std::string GetName()    const;

private:
    float time;
 
    std::vector<KeyPosition> positions;
    std::vector<KeyRotation> rotations;
    std::vector<KeyScale>    scales;

    int GetPositionIndex();
    int GetRotationIndex();
    int GetScaleIndex();

    float GetFactor(float lastTime, float nextTime);

    glm::mat4 InterpolatePosition();
    glm::mat4 InterpolateRotation();
    glm::mat4 InterpolateScale();
    glm::mat4 transform;
    std::string name;
};
#pragma once
#include <vector>
#include <string>

#include "../glm.h"

struct aiNodeAnim;

struct KeyPosition
{
    KeyPosition(glm::vec3 pos, float time) : position(pos), timeStamp(time) {}

    glm::vec3 position;
    float timeStamp;
};

struct KeyRotation
{
    KeyRotation(glm::quat rotation, float time) : orientation(rotation), timeStamp(time) {}

    glm::quat orientation;
    float timeStamp;
};

struct KeyScale
{
    KeyScale(glm::vec3 vec, float time) : scale(vec), timeStamp(time) {}

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
    Bone() = default;
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
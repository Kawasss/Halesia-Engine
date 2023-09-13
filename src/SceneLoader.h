#pragma once
#include "Scene.h"
#include <iostream>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include "glm.h"
#include "Vertex.h"

struct MeshCreationData
{
	std::string name;

	bool hasBones;
	bool hasMaterial;

	int amountOfVertices;
	std::vector<Vertex> vertices;
	//int amountOfIndices;
	std::vector<uint16_t> indices;
};

struct ObjectCreationData
{
	std::string name;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;

	int amountOfMeshes;
	std::vector<MeshCreationData> meshes;
};

class SceneLoader
{
public:
	SceneLoader(std::string sceneLocation)
	{
		this->location = sceneLocation;
	}

	Scene* LoadScene()
	{
		OpenInputFile(location);
		RetrieveHeader();
		RetrieveCameraVariables();
		RetrieveLightVariables();
		RetrieveObjectVariables();
		RetrieveAllObjects();
		return nullptr;
	}

	// camera related info
	glm::vec3 cameraPos;
	float cameraPitch, cameraYaw;

	// light related info
	// int amountOfLights; not impelemented yet
	glm::vec3 lightPos;

	// model related info
	int amountOfObjects;
	std::vector<ObjectCreationData> objects;

private:
	// file specific info
	std::string location;
	std::string header;
	std::ifstream stream;

	template<typename T> T GetType(char* bytes)
	{
		T f = 0;
		memcpy(&f, bytes, sizeof(T));
		return f;
	}

	glm::vec3 GetVec3(char* bytes)
	{
		glm::vec3 ret;
		ret.x = GetType<float>(bytes);
		ret.y = GetType<float>(bytes + 4);
		ret.z = GetType<float>(bytes + 8);
		return ret;
	}

	glm::vec2 GetVec2(char* bytes)
	{
		glm::vec2 ret;
		ret.x = GetType<float>(bytes);
		ret.y = GetType<float>(bytes + 4);
		return ret;
	}

	std::string RetrieveName()
	{
		char nameBytes[11];
		stream.read(nameBytes, 10);
		return (std::string)nameBytes;
	}

	glm::vec3 RetrieveTransformData()
	{
		char posBytes[12];
		stream.read(posBytes, 12);
		return GetVec3(posBytes);
	}

	Vertex RetrieveOneVertex()
	{
		Vertex ret; // a vertex is structures like this: position (12 bytes), uv coords (8 bytes), normal (12 bytes). add that up and you get 32
		char vecBytes[32];
		stream.read(vecBytes, 32);
		ret.position = GetVec3(vecBytes);
		ret.textureCoordinates = GetVec2(vecBytes + 12);
		ret.normal = GetVec3(vecBytes + 20);

		return ret;
	}

	void RetrieveOneMesh(MeshCreationData& creationData)
	{
		creationData.name = RetrieveName();
		for (int i = 0; i < 3; i++) // the file contains the transform data of the meshes, but those are already processed into the objects transformation, so ignore these
			RetrieveTransformData();

		char containsBonesOrTextures;
		stream.read(&containsBonesOrTextures, 1);
		creationData.hasMaterial = GetType<bool>(&containsBonesOrTextures);
		stream.read(&containsBonesOrTextures, 1);
		creationData.hasBones = GetType<bool>(&containsBonesOrTextures);

		char amountOfVerticesBytes[4];
		stream.read(amountOfVerticesBytes, 4);
		creationData.amountOfVertices = GetType<int>(amountOfVerticesBytes);
		
		for (int i = 0; i < creationData.amountOfVertices; i++)
		{
			creationData.vertices.push_back(RetrieveOneVertex()); // could do resize like in RetrieveOneObject, but one mesh can have thousands of vertices so i dont know about the copying speed of that
			creationData.indices.push_back(i); // since CRS doesnt support indices (yet), this is filled like there are no indices
		}
	}

	void RetrieveOneObject(int index)
	{
		ObjectCreationData& creationData = objects[index];

		creationData.name = RetrieveName();
		creationData.position = RetrieveTransformData();
		creationData.scale = RetrieveTransformData();
		creationData.rotation = RetrieveTransformData();
		
		char amountOfMeshesBytes[4];
		stream.read(amountOfMeshesBytes, 4);
		creationData.amountOfMeshes = GetType<int>(amountOfMeshesBytes);
		creationData.meshes.resize(creationData.amountOfMeshes);

		for (int i = 0; i < creationData.amountOfMeshes; i++)
			RetrieveOneMesh(creationData.meshes[i]);
	}

	void RetrieveAllObjects()
	{
		for (int i = 0; i < amountOfObjects; i++)
			RetrieveOneObject(i);
	}

	void RetrieveObjectVariables()
	{
		char amountBytes[4];
		stream.read(amountBytes, 4);
		amountOfObjects = GetType<int>(amountBytes);

		objects.resize(amountOfObjects);
	}

	void RetrieveCameraVariables()
	{
		char cameraBytes[20];
		stream.read(cameraBytes, 20);
		cameraPos = GetVec3(cameraBytes);
		cameraPitch = GetType<float>(cameraBytes + 12);
		cameraYaw = GetType<float>(cameraBytes + 16);
	}

	void RetrieveLightVariables()
	{
		char lightBytes[12];
		stream.read(lightBytes, 12);
		lightPos = GetVec3(lightBytes);
	}

	void RetrieveHeader()
	{
		char lHeader[101];
		stream.read(lHeader, 100);
		this->header = std::string(lHeader);
	}

	void OpenInputFile(std::string path)
	{
		stream.open(path, std::ios::in | std::ios::binary);
		if (!stream)
			throw std::runtime_error("Failed to open the scene file at " + path + ", the path is either invalid or outdated / corrupt");
	}
};
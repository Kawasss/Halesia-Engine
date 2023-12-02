# Halesia Game Engine
Halesia is a multi-threaded 3D game engine with a focus on using 100% of the PC, asynchronous resource loading and ray tracing with Vulkan. It allows the programmer to customize objects like the game window, renderer and related parameters.

## Instance

The engine works by creating an instance at the beginning of the program. Some information has to be given in other to create such an instance: window size and name, playing an intro movie and more settings to fine-tune. All of these settings are read from a structure and will determine the engines behavior at runtime.

```
static void GenerateHalesiaInstance(HalesiaInstance& instance, HalesiaInstanceCreateInfo& createInfo);
```

The instance will keep track of all of its components: the engine will take care of everything once the ```Run()``` function has been called. It will also evaluate if the PC can handle the engine. There are some requirements, like ray-tracing that a lot of machines do not support. 

### Errors
The instance can stumble upon an error and exit with an error code. These are the most common exit codes:

```
enum HalesiaExitCode
{
	HALESIA_EXIT_CODE_SUCESS,
	HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION,
	HALESIA_EXIT_CODE_EXCEPTION
};
```

```HALESIA_EXIT_CODE_SUCESS``` means that the game engine has been told to exit by the game. This could come from many sources, like closing the game window or requesting it to exit via a function. ```HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION```  is returned when an unknown exception has been caught, this is often from an exception that does not inherit from ```std::exception```. ```HALESIA_EXIT_CODE_EXCEPTION``` is given when an exception has been thrown within the game or engine. The message and error type will automatically be displayed in a message box upon catch.

These error codes can be converted to a string with this function:
```
inline std::string HalesiaExitCodeToString(HalesiaExitCode exitCode);
```

## Scenes

The management of these objects happen within a scene. An object for this can easily be created by creating a class which inherits from the Scene class. This grants access to these functions and all of the events happening within a scene. To use a custom scene, it must be passed to the instance through the creation information given to ```GenerateHalesiaInstance```

### Loading objects

Different objects can be loaded in their own ways, objects with scripts can for example be loaded with this function, where you can choose to load the object via a file, like .obj, or internally via a scene file:

``` 
template<typename T> Object* AddCustomObject(std::string name, ObjectImportType objectType = OBJECT_IMPORT_INTERNAL);
```

Objects without any scripts can easily be loaded via this function, which only needs to data to create an object:

```
Object* AddStaticObject(const ObjectCreationData& creationData);
```

There are tons of functions with getting data for all kinds of purposes. The loader, for example, has a useful function for getting the creation data:

```
 ObjectCreationData LoadObjectFile(std::string path);
```

Objects can also be duplicated: this will only copy data, like meshes and materials, from one object to another, meaning that some resources will be shared to reduce memory usage. Components like transformations however aren't copied over.

```
 template<typename T> Object* DuplicateCustomObject(Object* objPtr, std::string name);
```

### Loading rendering resources

Every rendering resource, like textures and meshes, need an extra parameter in order to be created. often referred to as ```const VulkanCreationObject& creationObject```, this parameter can be easily fetched by calling the function pointer ```MeshCreationObject(*GetVulkanCreationObjects)();```. This wil automatically get the correct parameter needed to create any rendering resource.

### Getting objects

Objects will always be tied to a scene, as long as they are created with functions described in the "Loading objects" paragraph. It is because of this that the scenes do not need to keep track of objects manually. Instead, there are functions to help find objects.

There are two functions for this, but there are some important differences in their uses.

```
Object* GetObjectByName(std::string name);
```

This function iterates through all of the objects until it finds one that has a matching name. It will return when it has found the first object with the correct name, so any other object with the same name will not be returned. This should only be used if it can be guaranteed that only a single object is using that name.

Another way of founding objects is by using completely unique handles. Every object has an unique handle, for which it is guaranteed that no other resource uses that same handle. These handles are used for mouse picking and preventing unnecessary duplication.

```
Object* GetObjectByHandle(Handle handle);
```

This function will return a pointer to the object using the handle, but it will throw an ```std::runtime_error``` if the handle is not used by any object. One of the primary uses of this functions is mouse picking; it can turn mouse picking into a one-liner: 

```
Object* selectedObject = GetObjectByHandle(Renderer::selectedHandle);
```

This line of code can, however, throw an error if the mouse is not hovering over an object. Luckily, there is a function to prevent this. This simply checks all objects to see if any uses the given handles. if none use the handle it returns false.

```
bool IsObjectHandleValid(Handle handle);
```

## Objects

Objects are at the core of Halesia: everything inside of a scene is an object. Every object has the same core components that dictate the behavior of an object, like its transformation, meshes and state. The transformation handles the rotation, position and scaling of an object, all of which can be changed externally. The meshes, on the other hand, take care of the rendering part of an object. They hold the variables which decide how to object looks, like the vertices, indices and materials. The state determines how an object is handled each update. There are three settings for this:

```
enum ObjectState 
{
	OBJECT_STATE_VISIBLE,
	OBJECT_STATE_INVISIBLE,
	OBJECT_STATE_DISABLED
}
 ```

The use case for these states is pretty straight-forward: ```OBJECT_STATE_VISIBLE``` calls the update function and renders it, ```OBJECT_STATE_INVISIBLE``` calls the update function, but does not render it, while ```OBJECT_STATE_DISABLED``` does neither.

This enum also comes with a function to convert the value to a string: 

```
static std::string Object::ObjectStateToString(ObjectState state);
```

### Scripts

No function returns a pointer to a script due to abstraction, so this has to be done via the base class:

```
template<typename T> T GetScript();
```

This function returns a pointer to the given class. If a custom object has been created with the ```RotatingCube``` class as a script, then that class can be retrieved from the base class pointer with ```ObjPtr->GetScript<RotatingCube*>();``` This function will throw an ```std::runtime_error``` if no script can be found. If a script can not be guaranteed, then the function ```HasScript()``` should be called to check if there is a script for the object.

### Overriding base functions

Multiple functions can be overridden by derived object classes, these can be used for initialisation, updates or destruction:

```
virtual void Start();
virtual void Update(float delta);
virtual void Destroy();
```

### materials

A material consists solely of pointers to textures. There is a total of five different texture types, as is shown in this enum:

```
enum MaterialTexture
{
	MATERIAL_TEXTURE_ALBEDO,
	MATERIAL_TEXTURE_NORMAL,
	MATERIAL_TEXTURE_METALLIC,
	MATERIAL_TEXTURE_ROUGHNESS,
	MATERIAL_TEXTURE_AMBIENT_OCCLUSION
};
```

This enum can also be used to index the material like this:

```
Texture* normalTexture = material[MATERIAL_TEXTURE_NORMAL];
```

This snippet will assign the materials normal texture to the variable ```normalTexture```.

There is by default a standard "placeholder" texture for each material type. These are called ```Texture::placeholder``` + usage, i.e. ```Texture::placeholderAlbedo``` for the default albedo texture. These are automatically set when a material is created.

The material of any mesh can be freely changed via the function ```void SetMaterial(Material material);```. This will immediately set the material of the mesh to the given one. The material can also be reset to the default material by calling ```void ResetMaterial();```

## Camera

A new camera can be created in the same way as an object: any class that inherits from the ```Camera``` class can be used as a camera in the scene. There are no functions for adding a camera to the scene in the same way that an object has.

A key difference between an object and a camera is the way its transformation is calculated. Objects use a ```Transform``` to keep track of that, while a camera does not use a ```Transform```. Instead, it uses variables that are optimal for manipulating a camera:

```
float pitch, yaw, fov;
glm::vec3 position, up, right, front;
```

```pitch``` and ```yaw``` are used to rotate the camera around, while ```fov```, which is short for field of view, decides how much the camera is zoomed out. ```position``` decides where to place the camera. ```up``` and ```right``` are supporting values that generally are not calculated manually, since they can be automatically derived from front. To calculate the ```up``` and ```right``` from the normal call this function:

```
void UpdateUpAndRightVectors();
```

The ```pitch``` and ```yaw``` can be difficult to assign the correct values without help, because they have to be clamped and converted to radians. To combat this issue there are two functions:

```
void SetPitch(float newPitch);
void SetYaw(float newYaw);
```

## Input

## Tools

### Timer
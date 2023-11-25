# Halesia Game Engine
Halesia is a multi-threaded game engine with a focus on using 100% of the PC, asynchronous resource loading and ray tracing with Vulkan. It allows the programmer to customize objects like the game window, renderer and related parameters.

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

### Overriding base functions

Multiple functions can be overridden by derived object classes, these can be used for initialisation, updates or destruction:

```
virtual void Start();
virtual void Update(float delta);
virtual void Destroy();
```

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
 template<typename T> Object* DuplicateObject(Object* objPtr, std::string name);
```

## Scenes

The management of these objects happen within a scene. An object for this can easily be created by creating a class which inherits from the Scene class. This grants access to these functions and all of the events happening within a scene. To use a custom scene, it must be passed to the instance through the creation information given to ```GenerateHalesiaInstance```

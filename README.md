
# 🚀 C++ Template Project  

##  Usage  

This project uses **CMake** for building.
In the root of the project run the following commands in a terminal window.
### Configure (Set Up the Build)  

```sh
cmake --preset <configure preset name> 
``` 

The available configure presets are:
- debug
- Release 

###  Build (Compile the Project)  

After configuring, build using the corresponding preset:  

```sh
cmake --build --preset <build preset name>  
```  

The available build presets corresponding to your configure preset are:
- build-debug (if you used the debug preset)
- build-release (if you used the release preset)

The executable will then be in the following location:

```
project_root/build/_exec
```



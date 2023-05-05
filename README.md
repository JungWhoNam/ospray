# Extending OSPRay to support off-axis projection
> This project is part of a larger project called [Immersive OSPray](https://github.com/jungwhonam-tacc/Whitepaper).

We extend [OSPRay v2.10.0](https://github.com/ospray/ospray/releases/tag/v2.11.0) to support off-axis projection enabling us to display a single, coherent 3D virtual environemnt on non-planar, tiled-display walls.

## Building OSPRay extension
> Make sure to use the branch [v2.11.0-alpha.x](https://github.com/jungwhonam-tacc/ospray/tree/v2.11.0-alpha.x).

```
git clone https://github.com/jungwhonam-tacc/ospray.git
cd ospray

# switch to the alpha branch
git checkout v2.11.0-alpha.x

# create "build/release"
mkdir build
cd build
mkdir release

cmake -S ../scripts/superbuild -B release -DCMAKE_BUILD_TYPE=Release -DBUILD_OSPRAY_MODULE_MPI=ON -DBUILD_EMBREE_FROM_SOURCE=OFF

cmake --build release -- -j 5

cmake --install release
```

## Implementation Details
```PerspectiveCamera``` is extended so that one can 
- Define an image plane that is not orthogonal to the camera's viewing direction
- Specify an image plane by giving positions of three corners of the plane
> In our case, three corners of an image plane would be corners of a physical display.

Four variables are added to the structure. ```offAxisMode``` indicates the current viewing mode. The three ```vec3f``` variables are the positions of the three corners.

```
// modules/cpu/camera/PerspectiveCamera.h

bool offAxisMode{false};
vec3f topLeft;
vec3f botLeft;
vec3f botRight;
```

 If ```offAxisMode``` is ```false```, the camera behaves like the original mode; it uses the field of view, viewing direction, etc., to compute an image plane. However, when ```offAxisMode``` is ```true```, the camera creates an image plane using the three corners. 

```
// modules/cpu/camera/PerspectiveCamera.cpp

if (offAxisMode) {
  getSh()->du_size = botRight - botLeft;
  getSh()->dv_up = topLeft - botLeft;
  getSh()->dir_00 = botLeft - getSh()->org;
}
```

## Bug Fixes
This alpha version fixes few bugs. See the list in https://github.com/jungwhonam-tacc/ospray/issues.
* problems with snappy: https://github.com/jungwhonam-tacc/ospray/issues/1
* problems with finding libtbb: https://github.com/jungwhonam-tacc/ospray/issues/2
* problems with finding openvkl: https://github.com/jungwhonam-tacc/ospray/issues/2
* program crashes when pressing 'g': https://github.com/jungwhonam-tacc/ospray/issues/4

## Future Work
The off-axis projection feature is built on top of ```PerspectiveCamera.cpp```. Depending on the current mode, there are variables not being used. We plan to create another struct object, called ```OffAixProjectionCamera```, to keep the original camera intact and move the off-axis projection-related stuff.
# Scene Nodes
At the top level, every scene needs to specify the following:
- `width`: image width
- `height`: image height
- `spp`: samples per pixel
- `camera`: determines position and direction of primary rays (see [camera](#camera))
- `integrator`: samples the incoming light at a given position and direction (see [integrator](#integrator))

The scene must also contain at least one primitive. 

## Camera
For `perspective` and `orthographic` cameras a view transform needs to be specified and can be one of the following nodes:
- `matrix`: a 3x4 matrix (as in the simple case), with an optional `inverse` attribute, set to `false` by default
- `look_at`: look at transform with the following attributes:
  - `position`: position of the camera
  - `target`: point at the camera center
  - `up`: up direction

### Perspective
Attributes:
- `fovy`: vertical field-of-view in degrees
  - default: `45` 
- `view_transform`: world-to-camera transform, as described above

### Orthographic
Attributes:
- `view_transform`: world-to-camera transform, as described above

## Integrator
Integrators handle computing the incoming light at a given position in a given direction. 
### Normals
Debug integrator which computes the surface normal at the intersection of the primary ray.

### Random Walk
Attributes:
- `max_depth`: maximum number of bonces
	- default: `5`

### Simple Path
Attributes:
- `max_depth`: maximum number of bonces
	- default: `5`
- `sample_lights`: determines whether the integrators uses next event estimation
	- default: `true`
- `sample_bsdf`: determines whether the integrators importance samples the brdf
	- default: `true`

## Shape
### Sphere
Attributes:
- `radius`: radius of the sphere
- `center`: center of the sphere

### Triangle Mesh
Attributes:
- `indices`: list of position indices
- `positions`: list of triples of values representing a 3D position
- `to_world`: 3x4 transform matrix as an array of values in row-major order
  - default: the identity transform

### Obj Mesh
Mesh stored in a *.obj* file.

Attributes:
- `filename`: location of the mesh. The path is relative to the scene file
- `to_world`: 3x4 transform matrix as an array of values in row-major order
  - default: the identity transform

## BSDF
### Diffuse
Attributes:
- `albedo`: rgb triplet representing the surface color

### Dielectric
Attributes:
- `int_ior`: the internal index of refraction
	- default: `1.5046` (index of refraction of glass)
- `ext_ior`: the external index of refraction
	- default: `1.000277` (index of refraction of air)

### Mirror
Perfect mirror BSDF.

## Emitter
### Area
Attributes:
- `radiance`: emitted radiance

## Primitive
### Object
Attributes:
- `shape`: see [shape](#shape)
- `bsdf`: see [bsdf](#bsdf)
- `emitter`: see [emitter](#emitter)
  - default: no emitter

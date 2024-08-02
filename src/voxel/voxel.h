#ifndef VOXEL_H
#define VOXEL_H

#define VOXEL_PX_FACE_INDEX 0u
#define VOXEL_NX_FACE_INDEX 1u
#define VOXEL_PY_FACE_INDEX 2u
#define VOXEL_NY_FACE_INDEX 3u
#define VOXEL_PZ_FACE_INDEX 4u
#define VOXEL_NZ_FACE_INDEX 5u

#define NUM_CUBE_VOXEL_FACES 6u
#define NUM_CUBE_VOXEL_FACE_VERTICES 6u

#define NUM_CUBE_VOXEL_VERTICES (NUM_CUBE_VOXEL_FACES * NUM_CUBE_VOXEL_FACE_VERTICES)

#define VOXEL_TYPE_AIR 0u
#define VOXEL_TYPE_GRASS 1u
#define VOXEL_TYPE_STONE 2u
#define VOXEL_TYPE_DIRT 3u

#endif
#pragma once
#ifdef _MSC_VER
#pragma warning(disable : 4244 4996 4251 4275 4267)
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#pragma clang diagnostic ignored "-Wdeprecated-copy"
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

#define NANOVDB_USE_OPENVDB

#include <nanovdb/NanoVDB.h>
#include <nanovdb/util/HostBuffer.h>
#include <nanovdb/util/IO.h>
#include <nanovdb/util/OpenToNanoVDB.h>
#include <openvdb/Grid.h>
#include <openvdb/io/File.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Mask.h>
#include <openvdb/tools/MeshToVolume.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tree/Tree.h>
#include <openvdb/util/Util.h>

namespace gladius::io
{
    using UnsignedCharTree = openvdb::tree::Tree4<uint8_t, 5, 4, 3>::Type;
    using OneChannelCharGrid = openvdb::Grid<UnsignedCharTree>;
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
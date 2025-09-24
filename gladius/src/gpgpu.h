#pragma once
#define GPGPU_H
#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Prevent socket redefinition errors by defining these before Windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Prevent winsock.h from being included which conflicts with winsock2.h
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <Windows.h>
#include <iostream>
#endif

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>

#include <GLFW/glfw3.h>
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 120
#endif

#include <CL/opencl.h>
#include <CL/opencl.hpp>

#ifdef __linux__
#include <GL/glx.h>
#endif

#ifdef Success
#undef Success
#endif
#ifdef Status
#undef Status
#endif
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif

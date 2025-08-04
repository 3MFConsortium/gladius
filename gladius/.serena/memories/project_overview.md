# Gladius Project Overview

## Project Purpose
Gladius is a development tool and playground for the Volumetric Extension of the 3MF file format. It serves as:

- **Framework for processing 3MF files** with volumetric extension (implicit geometries)
- **Graphical programming interface** for designing parts using Constructive Solid Geometry (CSG)
- **Rendering engine** for visualizing 3D models and volumetric data
- **API library** with bindings for C++, C#, and Python
- **Development playground** for the 3MF Consortium's volumetric specification

## Key Features
- Import/export 3MF files with volumetric extension
- Edit function graphs for implicit geometry design
- Create custom mathematical functions
- Visualize 3mf files with volumetric extension
- Generate contours from 3D models
- Export to multiple formats: openvdb, STL, SVG (contours), CLI (contours)
- MCP (Model Context Protocol) server for AI agent integration

## Technology Stack
- **Language**: C++20
- **Build System**: CMake with vcpkg for dependency management
- **Graphics**: OpenGL for rendering, OpenCL for computations
- **UI Framework**: ImGui with node editor for graphical programming
- **Package Manager**: vcpkg
- **Testing**: GTest framework
- **3D Libraries**: OpenVDB, OpenMesh, Eigen3
- **File Formats**: lib3mf for 3MF files, various export formats

## System Requirements
- OpenCL 1.2 capable GPU (or CPU runtime)
- OpenGL support for UI
- Windows 10/11 or Linux (tested on Debian 12 Bookworm)
- Modern hardware with up-to-date drivers

## Target Users
- 3D printing software developers
- Research teams working with volumetric 3D models
- Developers building applications that process implicit geometries
- Members of the 3MF Consortium developing volumetric extensions

## Project Status
Early development stage with active development. Part of the 3MF Consortium ecosystem.
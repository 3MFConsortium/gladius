# CAD View Shortcuts Added to Gladius

This document lists all the standard CAD view shortcuts that have been implemented in the Gladius application, following industry standards from software like Blender, 3ds Max, Maya, AutoCAD, and SolidWorks.

## Standard View Orientations

### Primary Views (Numpad)
- **Numpad 1**: Front View
- **Ctrl+Numpad 1**: Back View  
- **Numpad 3**: Right View
- **Ctrl+Numpad 3**: Left View
- **Numpad 7**: Top View
- **Ctrl+Numpad 7**: Bottom View
- **Numpad 0**: Isometric View
- **Numpad 5**: Toggle Perspective/Orthographic

### Alternative Views (Main Keyboard)
For keyboards without numpad:
- **1**: Front View
- **Ctrl+1**: Back View
- **3**: Right View  
- **Ctrl+3**: Left View
- **7**: Top View
- **Ctrl+7**: Bottom View

## Camera Navigation

### Pan Controls
- **Numpad 4**: Pan Left
- **Numpad 6**: Pan Right
- **Numpad 8**: Pan Up
- **Numpad 2**: Pan Down

### Rotation Controls
- **Shift+Numpad 4**: Rotate Left
- **Shift+Numpad 6**: Rotate Right
- **Shift+Numpad 8**: Rotate Up
- **Shift+Numpad 2**: Rotate Down

### Zoom Controls
- **Numpad +**: Zoom In
- **Numpad -**: Zoom Out
- **Ctrl+=**: Zoom In (Alternative)
- **Ctrl+-**: Zoom Out (Alternative)
- **Numpad ***: Zoom Extents (fit all objects)
- **Numpad /**: Zoom Selected (fit selected objects)
- **Ctrl+0**: Reset Zoom

## View Management

### View Operations
- **Period (.)**: Center View
- **Home**: Frame All Objects
- **Shift+Left Arrow**: Previous View
- **Shift+Right Arrow**: Next View
- **Ctrl+V**: Save Current View
- **Ctrl+Shift+V**: Restore Saved View

### Camera Modes
- **F**: Toggle Fly/Walk Mode
- **O**: Orbit Mode
- **P**: Pan Mode
- **Z**: Zoom Mode
- **Ctrl+R**: Reset Orientation (to isometric)

## Model Operations

### Compilation
- **F5**: Compile Model (existing)
- **F7**: Compile Implicit Function

### Node Editor
- **Ctrl+L**: Auto Layout Nodes
- **Space**: Create Node Popup

### View Settings
- **Ctrl+K**: Show Keyboard Shortcuts Dialog

## Context Sensitivity

The shortcuts are context-sensitive and will only work when the appropriate window has focus:

- **RenderWindow Context**: All camera view, navigation, and zoom shortcuts
- **ModelEditor Context**: Node editor shortcuts (auto layout, create node, compile)
- **Global Context**: File operations (New, Open, Save), shortcuts dialog

## Implementation Notes

- All shortcuts follow standard CAD application conventions
- Numpad shortcuts are prioritized as they are industry standard
- Alternative shortcuts are provided for keyboards without numpad
- View history is maintained (up to 20 views)
- Camera modes can be switched for different interaction styles
- All camera operations properly invalidate the view for rendering updates

This comprehensive set of shortcuts makes Gladius much more efficient for CAD professionals who are familiar with industry-standard navigation patterns.

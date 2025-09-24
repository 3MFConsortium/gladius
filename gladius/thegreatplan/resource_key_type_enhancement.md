# ResourceKey Enhancement: Resource Type Differentiation

## Overview

This enhancement extends the `ResourceKey` class to distinguish between different resource types, providing a robust solution to the resource ID collision problem and improving resource management throughout the codebase.

## Problem Statement

The original issue was that beam lattice and mesh resources shared the same resource ID in 3MF files, causing the beam lattice loading to be skipped due to resource ID collision detection. The initial fix used an arbitrary offset, but this approach had limitations:

1. **Fragile**: Relies on arbitrary large offsets that could still collide
2. **Non-scalable**: Adding new resource types would require more offsets
3. **Non-semantic**: The relationship between resource ID and type is not explicit

## Solution Design

### ResourceType Enumeration

Added a comprehensive `ResourceType` enum to classify all resource types:

```cpp
enum class ResourceType
{
    Unknown = 0,
    Mesh,
    BeamLattice,
    ImageStack,
    Vdb,
    Stl,
    Function,
    Material
};
```

### Enhanced ResourceKey Class

Extended `ResourceKey` to include resource type as a distinguishing factor:

#### Key Changes

1. **Constructor Enhancement**: Added resource type parameter to resource ID constructor
2. **Hash Calculation**: Include resource type in hash computation for proper uniqueness
3. **Display Names**: Enhanced display names to include resource type information
4. **Type Access**: Added getter method for resource type

#### Implementation Details

```cpp
class ResourceKey
{
public:
    explicit ResourceKey(ResourceId resourceId, ResourceType resourceType = ResourceType::Unknown);
    
    auto getResourceType() const { return m_resourceType; }
    
    std::size_t getHash() const
    {
        // Include resource type in hash calculation
        hash_combine(hashValue, static_cast<int>(m_resourceType));
        // ... other hash components
    }
    
private:
    ResourceType m_resourceType{ResourceType::Unknown};
};
```

## Implementation Changes

### 1. Core ResourceKey Enhancement

**File**: `src/ResourceKey.h`

- Added `ResourceType` enum with comprehensive resource classification
- Enhanced constructor to accept resource type parameter
- Updated hash calculation to include resource type
- Improved display name generation with type information
- Added utility function `resourceTypeToString()` for debugging and display

### 2. 3MF Importer Updates

**File**: `src/io/3mf/Importer3mf.cpp`

- **Mesh Loading**: Updated to use `ResourceType::Mesh`
- **Beam Lattice Loading**: Updated to use `ResourceType::BeamLattice` 
- **Image Resources**: Dynamic type selection between `ResourceType::Vdb` and `ResourceType::ImageStack`
- **Mesh References**: Updated mesh references to use proper typing

#### Key Changes

```cpp
// Before: Resource ID collision
auto key = ResourceKey(static_cast<int>(meshObject->GetModelResourceID()));

// After: Type-differentiated keys
auto meshKey = ResourceKey(static_cast<int>(meshObject->GetModelResourceID()), ResourceType::Mesh);
auto beamKey = ResourceKey(static_cast<int>(meshObject->GetModelResourceID()), ResourceType::BeamLattice);
```

### 3. Other Importers

**Files**: `src/io/ImporterVdb.cpp`, `src/Document.cpp`

- Updated VDB importer to use `ResourceType::Vdb`
- Updated Document mesh resource creation to use `ResourceType::Mesh`
- Updated image stack resource creation to use `ResourceType::ImageStack`

## Benefits

### 1. Semantic Clarity
- Resource types are now explicitly declared and self-documenting
- Clear separation between different kinds of resources
- Better debugging and logging with type information

### 2. Collision Prevention
- Different resource types with same ID are now distinguishable
- Hash calculation includes type, ensuring unique keys
- Eliminates the need for arbitrary ID offsets

### 3. Scalability
- Easy to add new resource types without collision concerns
- Type-safe resource management
- Future-proof design for additional resource classifications

### 4. Maintainability
- Centralized resource type definitions
- Clear API with explicit type requirements
- Better error messages and debugging information

## Testing Strategy

### 1. Collision Resolution
The enhanced `ResourceKey` resolves the original beam lattice loading issue:

```cpp
// These now generate different hash values:
auto meshKey = ResourceKey(123, ResourceType::Mesh);          // Hash includes Mesh type
auto beamKey = ResourceKey(123, ResourceType::BeamLattice);   // Hash includes BeamLattice type
```

### 2. Integration Test Validation
The existing integration test should now pass:

```cpp
TEST_F(GladiusLib_test, VariableVoronoi_LoadAssembly_BoundingBoxIsValid)
{
    // Expected: Both mesh and beam lattice resources load successfully
    // Expected: Bounding box calculations include beam lattice geometry
    // Expected: Valid finite coordinates instead of FLT_MAX/-FLT_MAX
}
```

### 3. Resource Manager Verification
- Verify that resources with same ID but different types are stored separately
- Confirm that resource retrieval works correctly with type differentiation
- Test resource existence checks work properly with new key system

## Migration Notes

### Breaking Changes
- `ResourceKey` constructor now requires resource type parameter for resource ID-based keys
- Existing code using `ResourceKey(resourceId)` needs to specify resource type
- Hash values will be different due to type inclusion in calculation

### Compatibility
- File-based resource keys remain unchanged
- Unknown resource type provides fallback for legacy code
- Display names maintain backward compatibility where possible

## Future Enhancements

### 1. Resource Type Validation
Could add compile-time or runtime validation to ensure resources match their declared types:

```cpp
template<typename ResourceT>
ResourceKey makeTypedKey(ResourceId id) {
    static_assert(/* ResourceT matches expected type */);
    return ResourceKey(id, getResourceTypeFor<ResourceT>());
}
```

### 2. Hierarchical Types
Could extend to support sub-types or hierarchical classification:

```cpp
enum class ResourceType { Mesh_Static, Mesh_Animated, BeamLattice_Uniform, BeamLattice_Variable };
```

### 3. Resource Relationships
Could add parent-child relationships for resources like beam lattices that belong to meshes:

```cpp
class ResourceKey {
    std::optional<ResourceKey> m_parentKey;
};
```

## Conclusion

This enhancement provides a robust, scalable solution to resource management that goes beyond fixing the immediate beam lattice collision issue. The type-aware `ResourceKey` system creates a foundation for better resource organization and prevents similar issues in the future.

The implementation maintains backward compatibility where possible while providing clear migration paths for existing code. The semantic clarity and debugging improvements make the codebase more maintainable and easier to understand.

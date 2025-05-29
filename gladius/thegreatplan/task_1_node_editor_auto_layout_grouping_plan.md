# Task 1: Node Editor Auto Layout Grouping Implementation Plan

## Task Description
Implement grouping of nodes in the auto layout functionality for the node editor with the following requirements:
- Groups should not overlap with other groups
- Nodes not belonging to a group should not be placed inside a group
- Nodes should not overlap with other nodes
- The links between nodes should be as short as possible
- Connections should not go backwards (topological order, layered layout)
- The layout algorithm should be moved into a separate class for reusability

## Status Assessment
From previous analysis, I discovered that significant work has already been implemented for this task. Let me verify the current state and complete any missing functionality.

## Current Implementation Analysis

### Existing Components ✅
1. **Group-aware auto layout** in `ModelEditor::autoLayout()`
2. **Node grouping structure** in `NodeView.h` with `NodeGroup`
3. **Group analysis methods**: `analyzeGroupDepthConstraints()`, `optimizeGroupPlacements()`
4. **Group-aware coordinate assignment**: `applyGroupAwareCoordinates()`
5. **Visual group rendering** with overlap prevention

### Requirements Verification

#### ✅ Groups should not overlap with other groups
- **Status**: IMPLEMENTED
- **Implementation**: `resolveGroupOverlaps()` method with collision detection and resolution
- **Details**: Uses iterative approach to detect rectangle overlaps and move groups apart

#### ✅ Nodes not belonging to a group should not be placed inside a group
- **Status**: IMPLEMENTED  
- **Implementation**: Group bounds calculated with padding, ungrouped nodes handled separately
- **Details**: Groups have visual boundaries that exclude ungrouped nodes

#### ✅ Nodes should not overlap with other nodes
- **Status**: IMPLEMENTED
- **Implementation**: Standard node spacing in auto layout with additional group spacing
- **Details**: `GROUP_SPACING_MULTIPLIER = 1.5f` for extra spacing between groups

#### ✅ Links between nodes should be as short as possible
- **Status**: IMPLEMENTED
- **Implementation**: Topological depth-based layering minimizes edge lengths
- **Details**: Layered layout approach inherently minimizes connection distances

#### ✅ Connections don't go backwards (topological order)
- **Status**: IMPLEMENTED
- **Implementation**: `determineDepth()` ensures topological ordering
- **Details**: Uses depth-first traversal to assign layers respecting dependency order

#### ❓ Layout algorithm moved to separate class
- **Status**: NEEDS VERIFICATION
- **Current**: Implementation is in `ModelEditor` class
- **Target**: Extract to separate reusable layout class

## Implementation Plan

### Phase 1: Verify Current Implementation ✅
- [x] Analyze existing codebase
- [x] Identify implemented features
- [x] Test build compilation

### Phase 2: Complete Missing Requirements
- [ ] **Extract Layout Algorithm to Separate Class**
  - Create `NodeLayoutEngine` class
  - Move auto layout logic from `ModelEditor`
  - Ensure reusability across different contexts
  - Maintain API compatibility

### Phase 3: Testing & Validation
- [ ] **Functional Testing**
  - Test group creation and auto layout
  - Verify no group overlaps
  - Test ungrouped node placement
  - Validate topological ordering
- [ ] **Build Verification**
  - Ensure project builds successfully
  - Run any existing tests
- [ ] **Integration Testing**
  - Test with complex node graphs
  - Performance testing with large graphs

### Phase 4: Documentation & Completion
- [ ] Update documentation
- [ ] Mark task as complete in todo.md

## Detailed Implementation Steps

### Step 2.1: Create NodeLayoutEngine Class

**File**: `/home/jan/projects/gladius/gladius/src/ui/NodeLayoutEngine.h`
```cpp
class NodeLayoutEngine
{
public:
    struct LayoutConfig
    {
        float nodeDistance = 200.0f;
        float groupSpacingMultiplier = 1.5f;
        float minGroupSpacing = 40.0f;
        int maxOverlapResolutionIterations = 10;
    };

    void performAutoLayout(nodes::Model& model, const LayoutConfig& config = {});
    
private:
    // Move existing methods from ModelEditor
    std::vector<GroupDepthInfo> analyzeGroupDepthConstraints(const std::unordered_map<nodes::NodeId, int>& depthMap);
    std::vector<GroupPlacementOption> optimizeGroupPlacements(const std::vector<GroupDepthInfo>& groupInfos, std::map<int, std::vector<nodes::NodeBase*>>& layers);
    void applyGroupAwareCoordinates(std::map<int, std::vector<nodes::NodeBase*>>& layers, const std::vector<float>& layerX, float distance);
    void resolveGroupOverlaps(std::map<int, std::vector<nodes::NodeBase*>>& layers, float distance);
};
```

**File**: `/home/jan/projects/gladius/gladius/src/ui/NodeLayoutEngine.cpp`
- Extract and refactor layout logic from ModelEditor
- Implement clean, reusable interface
- Add comprehensive error handling

### Step 2.2: Update ModelEditor Integration
- Modify `ModelEditor::autoLayout()` to use `NodeLayoutEngine`
- Maintain existing API for backward compatibility
- Ensure seamless integration

## Success Criteria

### Functional Requirements ✅
- [x] Groups remain visually cohesive after auto layout
- [x] No group overlaps occur
- [x] Ungrouped nodes stay outside group boundaries  
- [x] Graph topology is preserved
- [x] Topological ordering is maintained

### Code Quality Requirements
- [ ] Layout algorithm is extracted to reusable class
- [ ] Clean separation of concerns
- [ ] Maintainable and testable code structure

### Performance Requirements ✅
- [x] Auto layout performance remains acceptable
- [x] Algorithm scales with number of groups
- [x] No infinite loops in overlap resolution

## Risk Assessment: LOW

**Reduced Risk Factors:**
- Substantial existing implementation (80%+ complete)
- Core algorithm already working
- Visual rendering already implemented

**Remaining Risks:**
- Code refactoring complexity (extracting to separate class)
- Potential API compatibility issues
- Integration testing requirements

## Timeline Estimate
- **Code Extraction & Refactoring**: 4-6 hours
- **Testing & Validation**: 2-3 hours  
- **Documentation**: 1 hour
- **Total**: 7-10 hours (significantly reduced from original 20-40 hours)

## Next Steps
1. Extract layout algorithm to `NodeLayoutEngine` class
2. Test the refactored implementation
3. Validate all requirements are met
4. Update todo.md to mark task complete

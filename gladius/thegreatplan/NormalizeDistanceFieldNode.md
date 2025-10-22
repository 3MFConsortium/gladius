# NormalizeDistanceField Node Concept

## Overview
A `NormalizeDistanceField` node that takes a distorted signed distance field (SDF) and normalizes it to produce a proper distance field where the gradient magnitude is approximately 1.0. This is essential for implicit modeling where SDFs can become distorted through transformations, blending operations, or procedural generation, but accurate distance information is needed for offset operations, shell/wall thickness computation, or rendering.

## Motivation
In implicit modeling, signed distance fields are frequently subject to operations that distort the distance metric:
- **Non-uniform scaling** transforms change distance relationships
- **Blending operations** (smooth min/max) distort distances near blend regions
- **Domain warping** and **procedural noise** can significantly alter gradient magnitudes
- **CSG operations** with approximate distance functions accumulate errors

While these distorted fields still encode the surface (zero isosurface), they no longer represent true Euclidean distances. This causes problems for:
1. **Offset/shell operations**: Applying a uniform wall thickness requires knowing the true distance
2. **Collision detection**: Distance queries need accurate nearest-point information
3. **Ray marching**: Sphere tracing with distorted distances is inefficient or incorrect
4. **Rendering**: Normal computation and shading artifacts occur with non-normalized gradients

The `NormalizeDistanceField` node solves this by iteratively correcting the distance field using the gradient magnitude.

## Mathematical Foundation

### Distance Field Properties
A proper signed distance field $d(\mathbf{p})$ satisfies:
$$\|\nabla d(\mathbf{p})\| = 1$$

For a distorted field $\tilde{d}(\mathbf{p})$, we have:
$$\|\nabla \tilde{d}(\mathbf{p})\| \neq 1$$

### Normalization Algorithm
The normalization process uses the gradient magnitude to estimate the true distance:

$$d_{normalized}(\mathbf{p}) = \frac{\tilde{d}(\mathbf{p})}{\|\nabla \tilde{d}(\mathbf{p})\|}$$

This works because:
- If $\|\nabla \tilde{d}\| > 1$: the field is "compressed" (distances too small)
- If $\|\nabla \tilde{d}\| < 1$: the field is "stretched" (distances too large)
- Dividing by the gradient magnitude corrects the scale locally

### Edge Cases and Stability
1. **Zero gradient**: Near critical points (local minima/maxima), $\|\nabla \tilde{d}\| \approx 0$. We clamp to avoid division by zero.
2. **Far field**: At large distances from the surface, normalization may be less accurate. An optional distance threshold can preserve far-field values.
3. **Sharp features**: At surface discontinuities or edges, gradients may be undefined. The node should handle these gracefully.

## Node Design

### Class Definition
```cpp
class NormalizeDistanceField : public ClonableNode<NormalizeDistanceField> {
  public:
    NormalizeDistanceField();
    explicit NormalizeDistanceField(NodeId id);
    
    std::string getDescription() const override;
};
```

### Parameters (Inputs)
| Name | Type | Description | Default | Notes |
|------|------|-------------|---------|-------|
| `distance` | float | Input (possibly distorted) signed distance | required | The SDF to normalize |
| `pos` | float3 | Position at which to evaluate | required | Used for gradient computation |
| `stepsize` | float | Finite difference step for gradient | 1e-3 | Same as FunctionGradient |
| `epsilon` | float | Minimum gradient magnitude threshold | 1e-6 | Prevents division by zero |
| `maxdistance` | float | Optional distance clamp (0 = disabled) | 0.0 | Preserves far field if > 0 |

### Outputs
| Name | Type | Description |
|------|------|-------------|
| `distance` | float | Normalized signed distance |

### Category
`Category::Math` or `Category::Primitive` (depending on conceptual fit with other distance nodes)

## Implementation Approaches

### Approach 1: Inline Gradient Computation (Direct Implementation)
Compute the gradient of the input distance field directly in OpenCL using finite differences:

```opencl
// Evaluate gradient using central differences
float3 delta_x = (float3)(stepsize * 0.5f, 0.0f, 0.0f);
float3 delta_y = (float3)(0.0f, stepsize * 0.5f, 0.0f);
float3 delta_z = (float3)(0.0f, 0.0f, stepsize * 0.5f);

float d_center = distance; // Already evaluated at pos
float d_px = /* evaluate distance at pos + delta_x */;
float d_mx = /* evaluate distance at pos - delta_x */;
float d_py = /* evaluate distance at pos + delta_y */;
float d_my = /* evaluate distance at pos - delta_y */;
float d_pz = /* evaluate distance at pos + delta_z */;
float d_mz = /* evaluate distance at pos - delta_z */;

float3 gradient = (float3)(
    (d_px - d_mx) / stepsize,
    (d_py - d_my) / stepsize,
    (d_pz - d_mz) / stepsize
);

float grad_magnitude = length(gradient);
float safe_magnitude = fmax(grad_magnitude, epsilon);

// Normalize
float normalized_distance = d_center / safe_magnitude;

// Optional: clamp to preserve far field
if (maxdistance > 0.0f && fabs(d_center) > maxdistance) {
    normalized_distance = d_center;
}
```

**Pros**:
- Self-contained, single node
- No dependencies on other specialized nodes
- Efficient for simple distance inputs

**Cons**:
- Cannot directly compute gradient if `distance` input comes from a complex subgraph
- Requires re-evaluation of the distance field 6 times (at perturbed positions)
- Less flexible than composition approach

### Approach 2: FunctionGradient Integration (Composition Approach) - RECOMMENDED
Build the normalization using existing nodes, particularly the newly **extended** `FunctionGradient` node which now exposes:
- `vector` (float3): normalized gradient (backward compatible)
- `gradient` (float3): raw unnormalized gradient (**new**)
- `magnitude` (float): ||∇f|| gradient length (**new**)

**Graph structure**:
1. Input: distance field function (via FunctionCall or inline)
2. Use `FunctionGradient` node to compute gradient
3. Use `FunctionGradient.magnitude` output directly (no need for separate Length node)
4. Use `Max` node to clamp: `safe_magnitude = max(epsilon, gradient.magnitude)`
5. Use `Division` node: `normalized = distance / safe_magnitude`
6. Use `Select` node for optional far-field preservation

**Pros**:
- ✅ Leverages existing `FunctionGradient` infrastructure
- ✅ Efficient: gradient magnitude already computed, just reuse it
- ✅ Composition is transparent and debuggable in node editor
- ✅ Reuses tested gradient computation logic
- ✅ Educational: shows how normalization works
- ✅ No redundant computation (magnitude is computed once)

**Cons**:
- Requires the distance input to be packaged as a referenceable function
- More nodes in the graph (though can be managed/hidden)
- Slightly more overhead (though flattening may optimize)

### Approach 3: Hybrid (Recommended)
Implement as a **single atomic node** in OpenCL that internally uses gradient computation, **but** make its behavior and parameters consistent with `FunctionGradient`:

```cpp
class NormalizeDistanceField : public ClonableNode<NormalizeDistanceField> {
    // Inputs: distance, pos, stepsize, epsilon, maxdistance
    // Output: distance (normalized)
};
```

In `ToOCLVisitor`:
- Generate code similar to Approach 1
- BUT: requires the `distance` parameter to allow **gradient tracking**
- This means the distance input must be marked as requiring positional derivatives

**Challenge**: The node graph may not expose a way to "re-evaluate" the distance input at perturbed positions unless it's structured as a function.

### Recommended Solution: Meta-Node / Graph Transformation
Given the architecture's existing patterns (particularly the `LowerFunctionGradient` transformation) and the **newly extended FunctionGradient node** with `gradient` and `magnitude` outputs, implement `NormalizeDistanceField` as:

1. **Authoring node**: A special node type that appears in the UI
2. **Lowering pass**: Before flattening, transform it into a composition of standard nodes:
   - Wrap the distance input's source into a helper function (if not already a function)
   - Insert `FunctionGradient` node (using its `magnitude` output directly)
   - Insert `Max`, `Division`, `Select` nodes
   - Replace the `NormalizeDistanceField` node with the composed graph
3. **Standard codegen**: The lowered graph uses existing node codegen

This approach:
- ✅ Provides clean UI (single node)
- ✅ Reuses proven gradient infrastructure with new efficient magnitude output
- ✅ Transparent (lowered graph is visible/debuggable)
- ✅ Follows established pattern (see `FunctionGradientLowering.md`)
- ✅ No redundant computations (magnitude already available)

## Detailed Specification

### Node Lifecycle

#### 1. Construction
```cpp
NormalizeDistanceField::NormalizeDistanceField(NodeId id)
    : ClonableNode<NormalizeDistanceField>(NodeName("NormalizeDistanceField"), 
                                           id, 
                                           Category::Math)
{
    TypeRule rule = {
        RuleType::Default,
        InputTypeMap{
            {FieldNames::Distance, ParameterTypeIndex::Float},
            {FieldNames::Pos, ParameterTypeIndex::Float3},
            {FieldNames::StepSize, ParameterTypeIndex::Float},
            {FieldNames::Epsilon, ParameterTypeIndex::Float},
            {FieldNames::MaxDistance, ParameterTypeIndex::Float}
        },
        OutputTypeMap{
            {FieldNames::Distance, ParameterTypeIndex::Float}
        }
    };
    
    m_typeRules = {rule};
    applyTypeRule(rule);
    
    // Set defaults
    m_parameter[FieldNames::StepSize].setValue(1e-3f);
    m_parameter[FieldNames::Epsilon].setValue(1e-6f);
    m_parameter[FieldNames::MaxDistance].setValue(0.0f); // disabled
    
    m_parameter[FieldNames::StepSize].setInputSourceRequired(false);
    m_parameter[FieldNames::Epsilon].setInputSourceRequired(false);
    m_parameter[FieldNames::MaxDistance].setInputSourceRequired(false);
    
    updateNodeIds();
}
```

#### 2. Lowering Transformation (`LowerNormalizeDistanceField`)
A new transformation pass `LowerNormalizeDistanceField` runs after `LowerFunctionGradient` and before `OptimizeOutputs`:

**Algorithm**:
For each `NormalizeDistanceField` node `N` in model `M`:

1. **Check if distance input is already from a function**:
   - If `distance` parameter is wired from a `FunctionCall` node's scalar output: Extract that function
   - Otherwise: Create a helper function `F_distanceSource` that wraps the distance input source

2. **Create normalization graph**:
   ```
   gradient_node = FunctionGradient(functionId=F_distanceSource, 
                                    selectedOutput="distance",
                                    selectedInput="pos",
                                    stepsize=N.stepsize)
                                    
   // Use the new magnitude output directly (no Length node needed!)
   safe_magnitude = Max(A=gradient_node.magnitude, B=N.epsilon)
   
   normalized = Division(A=N.distance, B=safe_magnitude)
   
   // Optional far-field preservation
   distance_abs = Abs(A=N.distance)
   is_far = Select(A=distance_abs, B=N.maxdistance, C=0.0, D=1.0)
   final = Select(A=is_far, B=0.5, C=normalized, D=N.distance)
   ```

3. **Wire and replace**:
   - Connect all inputs from `N` to the composed graph
   - Rewire all consumers of `N.outputs[distance]` to `final.outputs[result]`
   - Remove `N` from model

4. **Handle edge cases**:
   - If `maxdistance <= 0`: Skip far-field preservation nodes
   - If distance source cannot be traced: Create a trivial pass-through (no normalization)

### Field Names
Add to `FieldNames`:
```cpp
static constexpr auto Epsilon = "epsilon";
static constexpr auto MaxDistance = "maxdistance";
```

Note: `StepSize` and `Distance` already exist, `Pos` exists.

## Integration with Existing Systems

### FunctionGradient Extension
The `FunctionGradient` node has been extended to expose additional outputs beyond the normalized gradient:
- `vector` (float3): Normalized gradient (existing, backward compatible)
- `gradient` (float3): Raw unnormalized gradient (**new**)
- `magnitude` (float): Gradient length ||∇f|| (**new**)

This extension enables efficient distance field normalization without redundant computation. The magnitude is already computed internally during normalization, so exposing it is essentially free.

### Optimization Pass
The lowering produces standard nodes, so `OptimizeOutputs` handles them normally. The intermediate `FunctionGradient` node will mark the distance output as consumed in the helper function. Unused outputs (gradient, magnitude, or vector) will be pruned if not connected.

### Graph Flattener
The helper function `F_distanceSource` may be inlined by `GraphFlattener`, which is fine. The `FunctionGradient` node itself won't be flattened (per gradient preservation rules).

### OpenCL Codegen
No special handling needed; all lowered nodes have existing `ToOCLVisitor` implementations.

### Command Stream
Command stream backend not initially supported (same limitation as `FunctionGradient`). Mark with TODO.

## UI Considerations

### Node Appearance
- **Display Name**: "Normalize Distance Field"
- **Category**: Math
- **Icon**: Could use a "wave with arrow" or "gradient equalization" symbol
- **Color**: Same as other math nodes

### Parameter Controls
| Parameter | UI Element | Range/Constraints |
|-----------|-----------|-------------------|
| `distance` | Input port | Required connection |
| `pos` | Input port | Required connection |
| `stepsize` | Float input/connector | > 0, default 1e-3 |
| `epsilon` | Float input/connector | > 0, default 1e-6 |
| `maxdistance` | Float input/connector | >= 0, default 0 (disabled) |

### Tooltips
- **distance**: "Distorted signed distance field to normalize"
- **pos**: "Position at which to evaluate the distance field"
- **stepsize**: "Step size for gradient computation (smaller = more accurate, slower)"
- **epsilon**: "Minimum gradient magnitude threshold (prevents division by zero)"
- **maxdistance**: "Preserve original distances beyond this threshold (0 = always normalize)"

### Validation
- Warn if `distance` input is not connected
- Warn if `pos` input is not connected
- Info message if `maxdistance > 0`: "Far field preservation enabled"

## Use Cases and Examples

### Example 1: Normalized Shell/Offset
```
Problem: Apply 2mm wall thickness to a scaled sphere

Graph:
1. Sphere(radius=10) -> SignedDistanceToMesh -> distance1
2. Scale(distance1, factor=2.0) -> distance2  // Non-uniform scale distorts
3. NormalizeDistanceField(distance2) -> distance_normalized
4. Offset(distance_normalized, offset=-2.0) -> final_shell
```

Without normalization, the 2mm offset would be distorted by the scaling factor.

### Example 2: Smooth Blend Correction
```
Problem: Union-smooth of two primitives creates distance distortion

Graph:
1. Sphere(pos1) -> d1
2. Box(pos2) -> d2
3. SmoothMin(d1, d2, k=0.5) -> d_blended  // Distorted near blend
4. NormalizeDistanceField(d_blended) -> d_normalized
5. Use d_normalized for accurate ray marching
```

### Example 3: Procedural Noise Correction
```
Problem: Domain-warped surface has inaccurate distances

Graph:
1. Pos -> Noise3D -> warp_vector
2. Pos + warp_vector -> warped_pos
3. Sphere(warped_pos) -> distorted_distance
4. NormalizeDistanceField(distorted_distance) -> corrected_distance
```

## Testing Strategy

### Unit Tests
Create `tests/unittests/NormalizeDistanceFieldNode_tests.cpp`:

1. **LoweringCreatesExpectedGraph**: Verify the lowering pass produces the expected node composition
2. **PassesThroughPerfectSDF**: Sphere SDF (already normalized) should remain unchanged
3. **CorrectScaledSDF**: Scaled sphere should normalize correctly
4. **HandlesZeroGradient**: Constant distance field should use epsilon clamping
5. **FarFieldPreservation**: Distances beyond `maxdistance` threshold should be unchanged
6. **GeneratesValidOCL**: Lowered graph should produce compilable OpenCL code

### Integration Tests
1. **OffsetAccuracy**: Compare offset results with/without normalization on distorted SDF
2. **VisualTest**: Render normalized vs. unnormalized field; check for artifacts
3. **PerformanceBenchmark**: Measure overhead of normalization (6 extra evaluations + division)

### Validation Tests
1. **GradientMagnitudeCheck**: Output SDF should have $\|\nabla d\| \approx 1$ (within tolerance)
2. **SurfacePreservation**: Zero isosurface should remain identical to input
3. **SignPreservation**: Inside/outside classification should be preserved

## Performance Considerations

### FunctionGradient Extension Benefits
By extending `FunctionGradient` to expose `gradient` and `magnitude` outputs:
- **Zero overhead**: These values are already computed internally; exposing them is free
- **Eliminates redundant Length node**: Direct access to magnitude saves one node and operation
- **Optimization-friendly**: `OptimizeOutputs` can prune unused outputs
- **Backward compatible**: Existing uses of `vector` output remain unchanged

### Computational Cost
- **6 additional distance evaluations** (for gradient via finite differences)
- **1 vector length computation** (3 multiplies, 2 adds, 1 sqrt)
- **1 division**
- **Total overhead**: ~7-8x the cost of a single distance evaluation

### Optimization Opportunities
1. **Adaptive step size**: Use larger steps far from surface, smaller steps near it
2. **Caching**: If multiple normalizations at nearby points, cache gradient
3. **Analytical gradients**: For known primitives, use exact gradient formulas (future enhancement)
4. **GPU parallelization**: Already handled by OpenCL backend

### When to Avoid Normalization
- If the distance field is already known to be accurate (e.g., direct primitive SDFs)
- If only the zero isosurface matters (e.g., surface extraction only)
- If performance is critical and approximate distances are acceptable

## Known Limitations

1. **Not iterative**: Single-pass normalization may not fully correct severely distorted fields. Future: add `iterations` parameter for repeated normalization.

2. **Far-field accuracy**: Normalization is most accurate near the surface. At large distances, errors may remain.

3. **Sharp features**: At edges and corners, gradients may be discontinuous. Normalization can smooth these artifacts.

4. **Performance**: 7-8x overhead per evaluation. For deeply nested evaluations, this compounds.

5. **Requires function context**: The lowering approach requires wrapping the distance input. Direct scalar inputs are less efficient.

## Future Enhancements

### Analytical Gradient Support
Extend `FunctionGradient` and `NormalizeDistanceField` to support analytical derivatives for known primitives:
```cpp
if (auto* sphere = dynamic_cast<SignedDistanceToMesh*>(distanceSource)) {
    // Use analytical gradient: normalize(pos - center)
}
```

### Iterative Normalization
Add a `iterations` parameter to perform multiple normalization passes:
```cpp
float d = input_distance;
for (int i = 0; i < iterations; ++i) {
    d = d / length(gradient(d));
}
```

### Distance Field Reinitialization
More sophisticated algorithms (e.g., Fast Marching Method, Hamilton-Jacobi solvers) could provide better accuracy but require different infrastructure.

### 3MF Serialization
Once the 3MF spec is extended, add:
```xml
<normalizedistancefield identifier="norm1" displayname="Normalize SDF">
    <in>
        <scalarref identifier="distance" ref="distorted_sdf.distance"/>
        <vectorref identifier="pos" ref="inputs.pos"/>
        <scalar identifier="stepsize">0.001</scalar>
        <scalar identifier="epsilon">1e-6</scalar>
        <scalar identifier="maxdistance">0.0</scalar>
    </in>
    <out>
        <scalar identifier="distance"/>
    </out>
</normalizedistancefield>
```

## Implementation Roadmap

### Phase 1: Core Implementation (Milestone 1)
- [ ] Add `NormalizeDistanceField` node class to `DerivedNodes.h/cpp`
- [ ] Add field names (`Epsilon`, `MaxDistance`) to `FieldNames`
- [ ] Implement `LowerNormalizeDistanceField` transformation pass
- [ ] Add distance source wrapping logic (helper function creation)
- [ ] Integrate lowering pass into `Document::refreshModel()` pipeline
- [ ] Unit tests for lowering and basic functionality

### Phase 2: Integration & Testing (Milestone 2)
- [ ] UI integration (node palette, tooltips, parameter controls)
- [ ] Integration tests (offset accuracy, visual tests)
- [ ] Performance benchmarking
- [ ] Documentation updates (user guide, examples)

### Phase 3: Optimization & Polish (Milestone 3)
- [ ] Adaptive step size heuristics
- [ ] Far-field preservation refinement
- [ ] Error handling and validation improvements
- [ ] Command stream TODO markers

### Phase 4: Advanced Features (Future)
- [ ] Iterative normalization support
- [ ] Analytical gradient optimization for primitives
- [ ] 3MF serialization (requires spec extension)

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Helper function creation fails for complex inputs | Medium | High | Fallback to pass-through; warn user |
| Performance overhead too high for real-time use | Low | Medium | Document performance characteristics; provide bypass option |
| Normalization insufficient for severely distorted fields | Medium | Low | Document limitations; consider iterative option |
| Integration with existing nodes breaks | Low | High | Comprehensive integration testing |
| User confusion about when to use | Medium | Low | Clear documentation and examples |

## Documentation Requirements

### User-Facing Documentation
Create `documentation/manuals/NormalizeDistanceFieldGuide.md`:
- What is distance field normalization?
- When and why to use it
- Step-by-step examples
- Parameter tuning guide
- Performance considerations
- Troubleshooting common issues

### Architecture Documentation
This document serves as the primary architecture reference. Additionally:
- Update `FunctionGradientLowering.md` to mention the similar pattern
- Add to node catalog in user guide
- Include in API reference

### Code Comments
- Inline comments in `LowerNormalizeDistanceField` explaining the transformation steps
- Docstrings for public methods
- TODO markers for future enhancements

## Conclusion

The `NormalizeDistanceField` node addresses a critical need in implicit modeling: recovering accurate distance information from distorted SDFs. By leveraging the **extended** `FunctionGradient` infrastructure (now with `gradient` and `magnitude` outputs) and following the established lowering pattern, the implementation can be robust, maintainable, and transparent.

The recommended approach (meta-node with graph lowering) provides the best balance of:
- **User experience**: Simple, single-node interface
- **Maintainability**: Reuses proven components
- **Transparency**: Lowered graph is inspectable
- **Performance**: Leverages existing optimization passes and avoids redundant computation
- **Efficiency**: Direct use of FunctionGradient's magnitude output eliminates extra Length node

This design enables advanced SDF operations (accurate offsets, shell generation, ray marching optimization) while maintaining consistency with the project's architecture and design patterns.

---

## Implementation Status

### Completed (FunctionGradient Extension)
The following files have been modified to extend `FunctionGradient` with additional outputs:

#### Modified Files:
1. **`src/nodes/nodesfwd.h`**
   - Added `FieldNames::Gradient` and `FieldNames::Magnitude` constants

2. **`src/nodes/DerivedNodes.cpp`**
   - Updated `FunctionGradient::initializeBaseParameters()` to include new output types
   - Updated `FunctionGradient::updateInternalOutputs()` to preserve gradient and magnitude outputs

3. **`src/nodes/LowerFunctionGradient.cpp`**
   - Added gradient and magnitude outputs to synthesized gradient model's End node
   - Wired raw gradient output (masked)
   - Wired magnitude output (masked)

4. **`src/nodes/ToOCLVisitor.cpp`**
   - Extended `visit(FunctionGradient&)` to emit gradient and magnitude outputs
   - Maintains backward compatibility with existing vector output
   - Supports inlining for all three outputs

### Changes Summary:
- **Backward Compatible**: Existing `vector` output behavior unchanged
- **Zero Overhead**: New outputs reuse already-computed intermediate values
- **Optimization Ready**: `OptimizeOutputs` can prune unused outputs
- **Tested Pattern**: Follows same structure as vector output

### Remaining Work (NormalizeDistanceField Node):
To complete the full normalization feature, the following still needs implementation:
- [ ] Create `NormalizeDistanceField` node class
- [ ] Implement `LowerNormalizeDistanceField` transformation pass
- [ ] Add distance source wrapping logic
- [ ] Integrate lowering pass into pipeline
- [ ] Add UI elements and node palette entry
- [ ] Create unit and integration tests
- [ ] Add user documentation

---

**Status**: FunctionGradient extension complete - Ready for NormalizeDistanceField implementation
**Author**: Extended based on FunctionGradient implementation
**Date**: 2025-10-16
**Related Documents**: `FunctionGradientLowering.md`, `gradient_node_plan.md`

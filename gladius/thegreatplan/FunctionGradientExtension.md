# FunctionGradient Node Extension - Implementation Summary

## Overview
Extended the `FunctionGradient` node to expose additional outputs beyond the normalized gradient vector. This enables efficient composition of gradient-based operations without redundant computation.

**Date**: 2025-10-16  
**Status**: Implemented and tested

## Motivation
The original `FunctionGradient` node computed:
1. Raw gradient via finite differences: `∇f = (∂f/∂x, ∂f/∂y, ∂f/∂z)`
2. Gradient magnitude: `||∇f|| = length(∇f)`
3. Normalized gradient: `∇f / ||∇f||`

However, it only exposed the normalized gradient as output. For operations like distance field normalization, we need direct access to the magnitude without recomputing it via a separate `Length` node on the normalized vector (which would give 1.0, not the original magnitude).

## Changes Summary

### New Outputs
The `FunctionGradient` node now exposes three outputs:

| Output | Type | Description | Status |
|--------|------|-------------|--------|
| `vector` | float3 | Normalized gradient (∇f / \|\|∇f\|\|) | Existing (backward compatible) |
| `gradient` | float3 | Raw unnormalized gradient (∇f) | **New** |
| `magnitude` | float | Gradient length (\|\|∇f\|\|) | **New** |

### Files Modified

#### 1. `src/nodes/nodesfwd.h`
Added new field name constants:
```cpp
static auto constexpr Gradient = "gradient";
static auto constexpr Magnitude = "magnitude";
```

#### 2. `src/nodes/DerivedNodes.cpp`
**`FunctionGradient::initializeBaseParameters()`**:
- Extended `OutputTypeMap` to include `Gradient` (Float3) and `Magnitude` (Float)

**`FunctionGradient::updateInternalOutputs()`**:
- Added preservation logic for `gradient` and `magnitude` outputs
- Creates output ports if they don't exist

#### 3. `src/nodes/LowerFunctionGradient.cpp`
**`synthesizeGradientModel()`**:
- Added `Gradient` and `Magnitude` outputs to synthesized model's End node
- Wired raw gradient output through masking multiplication (zeros out if gradient length < epsilon)
- Wired magnitude output through masking multiplication (zeros out if gradient length < epsilon)

Key additions:
```cpp
// Raw gradient output
auto * rawGradientMasked = model->create<Multiplication>();
rawGradientMasked->setDisplayName("raw_gradient_output");
linkOrThrow(*model,
            composeGradient->getOutputs().at(FieldNames::Result),
            rawGradientMasked->parameter().at(FieldNames::A));
linkOrThrow(*model,
            maskVector->getOutputs().at(FieldNames::Result),
            rawGradientMasked->parameter().at(FieldNames::B));
linkOrThrow(*model,
            rawGradientMasked->getOutputs().at(FieldNames::Result),
            end->parameter().at(FieldNames::Gradient));

// Magnitude output
auto * magnitudeMasked = model->create<Multiplication>();
magnitudeMasked->setDisplayName("magnitude_output");
linkOrThrow(*model,
            lengthNode->getOutputs().at(FieldNames::Result),
            magnitudeMasked->parameter().at(FieldNames::A));
linkOrThrow(*model,
            maskSelect->getOutputs().at(FieldNames::Result),
            magnitudeMasked->parameter().at(FieldNames::B));
linkOrThrow(*model,
            magnitudeMasked->getOutputs().at(FieldNames::Result),
            end->parameter().at(FieldNames::Magnitude));
```

#### 4. `src/nodes/ToOCLVisitor.cpp`
**`visit(FunctionGradient&)`**:
- Extended to emit `gradient` and `magnitude` outputs when used
- Reuses already-computed intermediate variables (`gradientVarName`, `gradientLenVarName`)
- Supports inlining for all three outputs independently

Key additions:
```cpp
// Emit raw gradient output (new)
auto gradientRawOutputIter = functionGradient.getOutputs().find(FieldNames::Gradient);
if (gradientRawOutputIter != functionGradient.getOutputs().end() && 
    gradientRawOutputIter->second.isUsed())
{
    bool const canInlineGradient = shouldInlineOutput(functionGradient, FieldNames::Gradient);
    if (canInlineGradient)
    {
        auto const key =
          std::make_pair(functionGradient.getId(), std::string(FieldNames::Gradient));
        m_inlineExpressions[key] = gradientVarName;
    }
    else
    {
        m_definition << fmt::format(
          "float3 const {0} = {1};\n", 
          gradientRawOutputIter->second.getUniqueName(), 
          gradientVarName);
    }
}

// Emit magnitude output (new)
auto magnitudeOutputIter = functionGradient.getOutputs().find(FieldNames::Magnitude);
if (magnitudeOutputIter != functionGradient.getOutputs().end() && 
    magnitudeOutputIter->second.isUsed())
{
    bool const canInlineMagnitude = shouldInlineOutput(functionGradient, FieldNames::Magnitude);
    if (canInlineMagnitude)
    {
        auto const key =
          std::make_pair(functionGradient.getId(), std::string(FieldNames::Magnitude));
        m_inlineExpressions[key] = gradientLenVarName;
    }
    else
    {
        m_definition << fmt::format(
          "float const {0} = {1};\n", 
          magnitudeOutputIter->second.getUniqueName(), 
          gradientLenVarName);
    }
}
```

### Tests Updated

#### `tests/unittests/FunctionGradient_tests.cpp`
1. **Updated `MirrorsReferencedArgumentsIntoGradient`**:
   - Changed output count assertion from 1 to 3
   - Added checks for `Gradient` and `Magnitude` outputs

2. **Added `ExposesGradientAndMagnitudeOutputs`**:
   - Verifies all three outputs are present
   - Checks correct types (Float3 for vector/gradient, Float for magnitude)

3. **Added `ToOclVisitorEmitsGradientAndMagnitudeWhenUsed`**:
   - Marks gradient and magnitude outputs as used
   - Verifies OCL code contains gradient and magnitude variables

#### `tests/unittests/LowerFunctionGradient_tests.cpp`
**Updated `LowersGradientIntoFunctionCall`**:
- Added checks for `Gradient` and `Magnitude` in lowered model outputs

## Implementation Details

### Zero-Gradient Handling
When the gradient magnitude is below epsilon (1e-8), all three outputs are masked:
- `vector`: Returns `(0, 0, 0)`
- `gradient`: Returns `(0, 0, 0)`
- `magnitude`: Returns `0.0`

This prevents division by zero and provides consistent behavior across all outputs.

### Performance Characteristics
- **Zero overhead**: Gradient and magnitude are already computed internally
- **Conditional emission**: Only used outputs generate code
- **Inlining support**: All outputs can be inlined when beneficial
- **Optimization-friendly**: `OptimizeOutputs` can prune unused outputs

### Backward Compatibility
- ✅ Existing `vector` output unchanged
- ✅ Existing codegen paths unchanged
- ✅ Node behavior identical when only `vector` is used
- ✅ No breaking changes to serialization (3MF not yet implemented)

## Usage Examples

### Example 1: Distance Field Normalization
```
FunctionGradient.magnitude → Max(epsilon) → Division(distance, magnitude) → normalized_distance
```
Before: Required separate Length node, which would recompute length of normalized vector (always 1.0)
After: Direct access to original gradient magnitude

### Example 2: Gradient-Based Shading
```
FunctionGradient.vector → use as surface normal (normalized)
FunctionGradient.gradient → use for gradient magnitude visualization
FunctionGradient.magnitude → use for sharpness/detail metric
```

### Example 3: Conditional Operations
```
FunctionGradient.magnitude → Select(threshold) → 
    if > threshold: use detailed computation
    else: use approximate method
```

## Testing Strategy

### Unit Tests
- [x] Output presence and types
- [x] Backward compatibility (vector output unchanged)
- [x] New outputs in lowered model
- [x] OCL emission for used outputs

### Integration Tests
- Existing tests pass without modification
- New outputs available in node editor
- Proper code generation in complex graphs

## Future Enhancements

### Potential Additions
1. **Hessian matrix**: Second derivatives for curvature analysis
2. **Directional derivative**: Gradient in specific direction
3. **Laplacian**: ∇²f for diffusion/smoothing operations

### Optimization Opportunities
1. **Fused gradient computations**: When multiple gradient operations share input
2. **Analytical gradients**: For known primitives, bypass finite differences
3. **Adaptive step size**: Adjust based on gradient magnitude

## Related Documents
- `FunctionGradientLowering.md`: Original gradient node lowering design
- `NormalizeDistanceFieldNode.md`: Primary use case for this extension
- `gradient_node_plan.md`: Original gradient node implementation plan

---

**Conclusion**: The FunctionGradient extension provides efficient access to intermediate gradient computations without breaking existing functionality or adding runtime overhead. This enables elegant composition patterns for gradient-based operations like distance field normalization.

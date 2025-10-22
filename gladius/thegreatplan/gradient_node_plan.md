# FunctionGradient Node Implementation Plan

## Overview
Implement a `FunctionGradient` node (UI display name can remain simply "Gradient") that computes the spatial gradient of a referenced implicit function output using central finite differences. The node mirrors the input arguments of a referenced function (like `FunctionCall`) but exposes only a single normalized vector output (`vector`). The gradient is computed with respect to a chosen vector input argument of the referenced function and a chosen scalar output parameter of that function.

## Goals
- Reference an existing function (by ResourceId) and mirror only its inputs (arguments).
- Provide a single configurable finite-difference step size parameter (`stepsize`, default `1e-3`).
- Let the user choose:
  - A scalar output (float) of the referenced function to differentiate.
  - A vector input (float3) argument of the referenced function to differentiate with respect to.
- Produce exactly one output: a normalized gradient vector (`FieldNames::Vector`).
- If the raw gradient length is zero or numerically tiny, output `(0,0,0)`.
- Skip 3MF serialization for now (future extension required in the 3MF spec / lib3mf bindings).
- OpenCL backend support (primary). Command-stream backend initially unsupported (documented).

## Non-Goals (Current Iteration)
- Command stream execution parity.
- 3MF import/export of gradient nodes.
- Automatic analytical differentiation optimizations.
- Adaptive step size heuristics.

## Data Model
### Parameters
| Name | Type | Description | Notes |
|------|------|-------------|-------|
| `functionID` | ResourceId | Referenced function resource | May come from ResourceId node or literal. |
| `stepsize` | float | Finite difference step size (h) | Default 1e-3, clamped to > 1e-8. |
| Function arguments (mirrored) | float / float3 / matrix / etc. | Arguments of referenced function Begin node except position handling is unchanged | Marked as arguments. |

### Outputs
| Name | Type | Description |
|------|------|-------------|
| `vector` | float3 | Normalized gradient (∂f/∂v) |

### Internal Properties (Not Ports/Inputs)
- `m_selectedScalarOutputName` : `std::string` — name of referenced scalar output.
- `m_selectedVectorInputName` : `std::string` — name of referenced vector input argument.

Validation ensures both names correspond to correct types; else gradient output produces zero vector.

## Field Names
Add to `FieldNames` in `nodesfwd.h`:
```cpp
static constexpr auto StepSize = "stepsize"; // parameter name
```
No new field names for selections (they are properties, not ports).

## Class Definition (DerivedNodes.h)
```cpp
class FunctionGradient : public ClonableNode<FunctionGradient> {
  public:
    FunctionGradient();
    explicit FunctionGradient(NodeId id);

    void resolveFunctionId();
    ResourceId getFunctionId() const;
    void setFunctionId(ResourceId);

    void setSelectedScalarOutput(const std::string& name);
    void setSelectedVectorInput(const std::string& name);
    const std::string& getSelectedScalarOutput() const;
    const std::string& getSelectedVectorInput() const;

    void setStepSize(float h); // clamps
    float getStepSize() const;

    void updateInputsAndOutputs(Model& referencedModel);
  std::string getDescription() const override;

  private:
  ResourceId m_functionId{};
  std::string m_selectedScalarOutputName; // must be scalar output of referenced function
  std::string m_selectedVectorInputName;  // must be vector input (float3) of referenced function
    void updateInternalOutputs();
};
```

Initialization logic (final decisions):
1. Create parameters: `functionID` (ResourceId, non-required) and `stepsize` (float, connectable, default 1e-3). No special handling in UI; it is edited exactly like any other float parameter and may be wired.
2. Create output port: `vector` (float3).
3. Apply type rule containing only these two fixed parameters (mirrored arguments added later).
4. Selected scalar/vector names are empty until user sets them; until then the node evaluates to a zero vector.

## Mirroring Inputs (updateInputsAndOutputs)
Similar to FunctionCall but:
- Iterate referenced model Begin node parameters / outputs (arguments) and add them as arguments (skip duplicates; update types if changed). Do not copy referenced outputs.
- Always maintain only one output port `vector`.
- Validate `m_selectedScalarOutputName` exists in referenced End node params and is float; else clear string.
- Validate `m_selectedVectorInputName` exists among mirrored arguments and is float3; else clear.

## Function Resolution
Reuse logic from `FunctionCall::resolveFunctionId` (optionally factor minimal shared static helper). If the input port is wired from a ResourceId node, extract; else use literal value.

## Optimization Integration
In `OptimizeOutputs::propagateUsedFunctionOutputs`, add visitor for `FunctionGradient` nodes:
- If gradient has valid `m_selectedScalarOutputName`, mark corresponding End node parameter consumed so OpenCL codegen will include that output variable in function signature.

## OpenCL Code Generation
Add `visit(FunctionGradient&)` to `ToOclVisitor`:
1. Validate configuration (function id + selected names + arguments mirrored). Ignore raw step size sign here.
2. If invalid: emit `float3 <unique_vector_name> = (float3)(0.0f,0.0f,0.0f);` and return.
3. Resolve raw step size expression `h_raw = resolveParameter(stepsizeParam)`; clamp in code: `float FG_<id>_h = fmax(fabs(h_raw), 1e-8f);`
4. Build argument expressions (resolveParameter). For selected vector input (allowing `pos`) cache once: `float3 FG_<id>_v = (float3)( ... );`
5. For each axis i in {x,y,z}: compute `delta = FG_<id>_h * 0.5f`; create plus/minus modified vector literals, call referenced function twice collecting `f<i>p`, `f<i>m` (six total calls). Calls pass a pointer to scalar output param; other outputs ignored.
6. Derivatives: `dx = (fxp - fxm)/FG_<id>_h;` etc.
7. Normalize with threshold 1e-12: `len = sqrt(dx*dx+dy*dy+dz*dz); vectorOut = (len > 1e-12f)?((float3)(dx,dy,dz)/len):(float3)(0.0f);`
8. Unique temp naming: prefix all temps with `FG_<id>_` to avoid collisions.

### Edge Cases
- Step size clamp: `FG_<id>_h = fmax(fabs(h_raw), 1e-8f)` guarantees non-zero denominator.
- Zero-length normalization threshold: `len > 1e-12f` (stability against floating noise).
- NaN/INF from referenced function propagate; we only guard the zero-length case (consistent with existing style).

## Command Stream Handling
For now: `ToCommandStreamVisitor::visit(FunctionGradient&)` is a no-op with a comment:
```cpp
// TODO(Gradient): Command stream backend not yet implemented.
```
Potential future design: emulate with sequence of existing commands + temporary parameter slots, but out of scope.

## Integration With GraphFlattener / Inlining Strategy
### Background
`FunctionCall` nodes are flattened (their referenced function graphs are physically integrated into the assembly model) before OpenCL code generation. Consequently the `ToOclVisitor::visit(FunctionCall&)` hook is effectively a no-op: by codegen time there are no semantic function call boundaries left for ordinary function calls.

### Challenge for FunctionGradient
`FunctionGradient` needs to evaluate a referenced function multiple times (6 evaluations for central differences) at different perturbed inputs. Full graph duplication (flattening 6 times) would explode node count and compilation time. Instead we want to retain an invocable form of the referenced function so we can call it with varied arguments at runtime.

### Strategy
Do **not** flatten (`integrate`) `FunctionGradient` nodes. Referenced functions remain available as callable kernels (their models are preserved). We accept duplication if a normal `FunctionCall` also triggers flattening of the same function.

### GraphFlattener Adjustments
1. Discovery: visit `FunctionGradient` nodes; add their function IDs to used set.
2. Preservation: skip deletion of models referenced by gradients.
3. Integration: do **not** integrate the gradient node or substitute its function.
4. Simplification: still allowed; selection revalidated afterwards.

### FunctionGradient Traversal Additions
Add an `OnTypeVisitor<FunctionGradient>` in:
* `findUsedFunctionsInModel`
* `findUsedFunctionsUsingDependencyGraph`

For each `FunctionGradient`:
```
gradientNode.resolveFunctionId();
auto fid = gradientNode.getFunctionId();
if (fid != 0) m_usedFunctions.insert(fid);
```
Also record `fid` in a `std::unordered_set<ResourceId> m_preservedByFunctionGradient;`.

### Code Generation Flow With FunctionGradient Present
1. Graph flatten finishes; referenced function models required by FunctionGradient are still present inside the assembly's function map.
2. `ToOclVisitor` is invoked over each model (as today) generating one OpenCL function per model.
3. The assembly model visitor encounters the `FunctionGradient` node and emits inline gradient code that calls the preserved referenced function's generated OpenCL function name multiple times.

### Referenced Function Signature Requirements
Mark selected scalar output as consumed (never unmark). `OptimizeOutputs` runs *before* flattening (`Document::updateFlatAssembly`), so the function keeps the scalar output pointer parameter.

### Validation Guarantees
During `updateInputsAndOutputs` of `FunctionGradient`:
* Confirm referenced model exists (failure leaves node inert returning zero vector).
* Ensure selected scalar output is present and type float in End node parameter map.
* Ensure selected vector input is present and type float3 in Begin node outputs.
* If invalid, clear selections so codegen emits safe zero output.

### Avoiding Double Inclusion
If a function is both referenced by a normal `FunctionCall` and a `FunctionGradient`, flattening will integrate one copy (through the function call) while still preserving the original model for the gradient. This can duplicate logic at runtime. Optimization (future): detect this pattern and either (a) skip integration for that function call (treat it like gradient) or (b) allow gradient to reuse integrated graph via incremental evaluation instrumentation. For now we accept duplication for simplicity.

### Performance Considerations
Retaining the referenced function rather than inlining it six times saves node bloat but does compile-time keep an extra function. Runtime still performs 6 calls; each call executes the full function body. Potential future optimization: generate a specialized fused gradient function that performs shared sub-expression reuse across the six evaluations.

### Summary of Required Code Changes (GraphFlattener)
| Area | Change |
|------|--------|
| Member Data | Add `std::unordered_set<ResourceId> m_preservedByFunctionGradient;` |
| Function Discovery | Add visitor for `FunctionGradient` nodes inserting referenced function IDs into both `m_usedFunctions` and `m_preservedByFunctionGradient`. |
| deleteFunctions() | Skip deletion if id in `m_preservedByFunctionGradient`. |
| flattenRecursive() | No integration logic for `FunctionGradient` (leave node untouched). |
| findUsedFunctionsUsingDependencyGraph() | Mirror additions for gradient nodes. |

### OpenCL Visitor Pseudocode (Updated)
```
void ToOclVisitor::visit(FunctionGradient & g) {
  if (!isOutPutOfNodeValid(g)) return;
  g.resolveFunctionId();
  auto refModel = m_assembly->findModel(g.getFunctionId());
  auto & outPort = g.getOutputs().at(FieldNames::Vector);
  if (!refModel || !g.isConfigured()) {
      m_definition << fmt::format("float3 {} = (float3)(0.0f,0.0f,0.0f);\n", outPort.getUniqueName());
      return;
  }
  // Step size
  auto hExpr = resolveParameter(g.parameter().at(FieldNames::StepSize)); // lowercase "stepsize"
  m_definition << fmt::format("float FG_{}_h = fmax(fabs({}), 1e-8f);\n", g.getId(), hExpr);
  // Base vector (selected input, may be 'pos')
  auto vecExpr = resolveParameter(*g.getSelectedVectorInputParameter()); // helper to access parameter
  m_definition << fmt::format("float3 FG_{}_v = (float3)({});\n", g.getId(), vecExpr);
  // Evaluate +/- for each axis (pseudo)
  // emitFunctionEval(axisChar, +delta/-delta)
  // ... produce fpx,fmx,fpy,fmy,fpz,fmz
  m_definition << fmt::format("float FG_{}_dx = (FG_{}_fpx - FG_{}_fmx)/FG_{}_h;\n", id,id,id,id);
  m_definition << fmt::format("float FG_{}_dy = (FG_{}__fpy - FG_{}__fmy)/FG_{}_h;\n", id,id,id,id);
  m_definition << fmt::format("float FG_{}_dz = (FG_{}__fpz - FG_{}__fmz)/FG_{}_h;\n", id,id,id,id);
  m_definition << fmt::format("float FG_{}_len = sqrt(FG_{}_dx*FG_{}_dx + FG_{}_dy*FG_{}_dy + FG_{}_dz*FG_{}_dz);\n", id,id,id,id,id,id,id);
  m_definition << fmt::format("float3 {} = (FG_{}_len > 1e-12f) ? ((float3)(FG_{}_dx,FG_{}_dy,FG_{}_dz)/FG_{}_len) : (float3)(0.0f,0.0f,0.0f);\n", outPort.getUniqueName(), id, id,id,id,id);
}
```

---
**Action Items Added**
- Extend plan todos: update GraphFlattener steps & preserve functions referenced by FunctionGradient.
- Implement visitors for function discovery including FunctionGradient.
- Add new test: `PreservesReferencedFunctionModel_ForFunctionGradient` ensuring flatten does not delete referenced model.


## UI (NodeView)
No bespoke UI elements required beyond existing parameter editing:
- `stepsize` appears as a normal float parameter (connectable). No extra slider logic; clamp happens only in generated code.
- Function selection identical to `FunctionCall`.
- Dropdowns:
  - Scalar Output: End node float parameters.
  - Vector Input: Begin node float3 outputs (including `pos`).
- Warning text (yellow) shown if configuration incomplete; output stays zero until valid.
- Only one output pin: `vector`.

## Validation Logic
`isConfigured()` returns true only if both selected names are non-empty and types valid. If invalid, codegen emits zero vector. We mark newly selected scalar output as consumed and never unmark previous ones (extra outputs are harmless).

## Testing Plan
Add `tests/unittests/FunctionGradientNode_tests.cpp` with:
1. `MirrorsInputsOnly` – after updateInputsAndOutputs, ensure only vector output exists and arguments match referenced inputs.
2. `InvalidSelectionClears` – removing referenced scalar output clears selection.
3. `GeneratesCentralDifferenceCode` – OCL string contains patterns: `+ delta`, `- delta`, `/ h;`, normalization code.
4. `ZeroGradientFallback` – constant referenced function yields code assigning normalized zero conditional (inspect code snippet for ternary or conditional expression).
5. `OptimizeMarksScalarOutputConsumed` – optimize pass sets selected scalar output consumed flag.

## Documentation
Create `documentation/architecture/FunctionGradientNode.md` summarizing design (this plan can be the source summary). Include limitations and future work (command stream, 3MF serialization, performance improvements like fused evaluations or analytical differentiation).

## Future 3MF Serialization (TODOs)
Insert placeholders:
- In Writer3mf.cpp & Importer3mf.cpp near FunctionCall case:
```cpp
// TODO(FunctionGradient-3MF): Add serialization once 3MF spec supports FunctionGradient node.
```
Schema additions likely needed: new node type enum, additional input for step size, properties for selected scalar/vector references.

## Performance Considerations
- 6 function evaluations per gradient node invocation; heavy if nested.
- Potential optimizations (future): reuse partial results, vectorized multi-axis evaluation, or integrate gradient generation inside referenced function (analytical derivatives).

## Risk & Mitigation
| Risk | Impact | Mitigation |
|------|--------|-----------|
| Referenced function changes invalidate selections | Silent wrong gradient | Revalidate every updateInputsAndOutputs; clear invalid selections. |
| Very small step size amplifies noise | Unstable gradient | Clamp & user guidance tooltip. |
| Command stream path divergence | Inconsistent behavior | Clearly documented limitation. |

## Implementation Order
1. FieldNames addition.
2. FunctionGradient class skeleton + registration.
3. Assembly update logic.
4. OptimizeOutputs extension.
5. OpenCL visitor implementation.
6. UI integration.
7. Tests.
8. Docs + TODO markers.

## Done Criteria
- Build succeeds.
- Tests for Gradient pass.
- Existing tests unaffected.
- OpenCL path generates code containing gradient logic.
- Documentation file present.

---
Prepared for implementation. Next step: apply code changes following the order above.

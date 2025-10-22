# FunctionGradient → Graph Lowering Plan

This document describes a pre-flatten transformation that turns each `FunctionGradient` node into a normal function (graph) implemented with standard nodes and `FunctionCall`s. After the transformation, the assembly contains no `FunctionGradient` nodes; it can flow through existing Optimize/Flatten/Codegen/Export pipelines without special handling.

## Background and motivation

- Today, OpenCL generation for `FunctionGradient` needs access to the referenced function model (by `functionId`). After flattening, this model may be integrated or deleted, so lookup fails in codegen.
- Special-case code in codegen creates maintenance burden and complicates serialization.
- Lowering `FunctionGradient` into a regular function graph removes the special case and unifies pipelines.

## Goals

- Replace every `FunctionGradient` node with a `FunctionCall` to a synthesized function that computes the gradient via central differences.
- Keep UI/authoring surface unchanged.
- Leverage existing passes: `OptimizeOutputs`, `GraphFlattener`, `ToOCLVisitor`, and 3MF export (later).
- Ensure correctness, type-safety, and reuse where possible.

Non-goals (for now):
- Analytical differentiation.
- Command stream support.
- 3MF serialization of a dedicated `FunctionGradient` node (the graph serializes fine, and native support will come later).

## High-level approach

Introduce a new pass that runs before flattening:

- For each `FunctionGradient` node `g` in the assembly:
  1. Validate configuration (referenced function ID, selected scalar output name, selected vector input name, step size available).
  2. Find the referenced model `R` using the unflattened assembly.
  3. Synthesize a new function model `G` (“gradient-of-R”), which:
     - Mirrors `R`'s inputs (names and types), adds a `StepSize` float input if not already present in `R`.
     - Outputs a single vector `vector` (float3), the normalized central-difference gradient of the selected scalar output of `R` with respect to the selected vector input.
     - Implements six `FunctionCall`s to `R` with perturbed vector inputs `v±h e_x`, `v±h e_y`, `v±h e_z`, then computes `(f+ - f-) / (2*h)` for each axis, normalizes with epsilon.
  4. Insert or reuse `G` in the assembly (caching by `(R.id, selectedOut, selectedVec)`), setting a stable resource ID.
  5. Replace `g` with a `FunctionCall` to `G`:
     - Wire all mirrored arguments the same way as on `g`.
     - Wire `StepSize` input from `g`'s `StepSize` parameter.
     - Reconnect all consumers of `g`'s `vector` output to the `FunctionCall`'s `vector` output.
     - Remove `g` from the model.

Then run `OptimizeOutputs` and `GraphFlattener` as usual.

## Pass design

- Name: `LowerFunctionGradient`.
- Files: `src/nodes/LowerFunctionGradient.h`, `src/nodes/LowerFunctionGradient.cpp`.
- Entry point:
  - `class LowerFunctionGradient { public: explicit LowerFunctionGradient(Assembly& a); void run(); };`
- When to run:
  - In `Document::refreshModel()` or wherever the temporary assembly copy (`assemblyToFlat`) is created: run `LowerFunctionGradient(assemblyToFlat).run()` before `OptimizeOutputs optimizer{&assemblyToFlat}; optimizer.optimize();` and flattening.

### Data flow and helpers

- Discovery: Iterate all models in the assembly (skip managed). For each model, iterate nodes and collect pointers to `FunctionGradient` nodes plus their parent model IDs.
- Referenced model lookup: Use `assembly.findModel(fid)` on the original (unflattened) assembly copy you’re transforming.
- Caching: Maintain `std::unordered_map<Key, ResourceId>` where `Key = (R.id, selectedScalarOutputName, selectedVectorInputName)`. If present, reuse existing gradient function; otherwise synthesize.
- Resource ID allocation:
  - Deterministic approach: `G.id = hash("grad:" + std::to_string(R.id) + ":" + out + ":" + vec)` with collision check.
  - Or incremental approach: call `assembly.addModelIfNotExisting(Gid)` and choose a free ID.

## Function synthesis (G)

Inputs and outputs:
- Inputs: mirror `R.getInputs()` exactly (names, types), plus `StepSize` float if not already present in mirrored set. All inputs should be marked as arguments and visible; `StepSize` should be modifiable and non-negative (clamped by user node or guarded in graph if desired).
- Outputs: one vector output named `FieldNames::Vector` of type float3.

Internal structure (nodes inside G):
1. Read base vector `v` from the mirrored vector input (the same name as selected in `g`).
2. Read step `h` from `StepSize` input.
3. Create 3 perturbed vectors in positive direction:
   - `vpx = v + (h, 0, 0)`, `vpy = v + (0, h, 0)`, `vpz = v + (0, 0, h)`.
4. Create 3 perturbed vectors in negative direction:
   - `vnx = v - (h, 0, 0)`, `vny = v - (0, h, 0)`, `vnz = v - (0, 0, h)`.
   - You can construct `(h,0,0)` etc. using `VectorFromScalar` + `ComposeVector` or directly by wiring constants and `StepSize`.
5. Add six `FunctionCall` nodes to `R`:
   - For each call, wire all mirrored arguments; for the selected vector argument, connect the corresponding perturbed vector.
   - Create local scalar captures for the selected scalar output (only that one needs to be consumed; `OptimizeOutputs` will trim the rest).
6. Compute central differences:
   - `gx = (fpx - fnx) / (2*h)`, similar for `gy`, `gz`.
7. Normalize:
   - `len = length((gx, gy, gz))`, result = `len > 1e-8 ? (gx, gy, gz) / len : (0, 0, 0)`.
8. Wire result to G’s output `FieldNames::Vector`.

Display/naming:
- `G.getModelName()`: `gradient_of_${Rname}_${out}_${vec}` (sanitized).
- Node display names optional for readability.

## Node replacement

For each `FunctionGradient g` in a parent model `M`:
- Create `FunctionCall FC` in `M`.
- Set `FC.functionId = G.id`.
- For every mirrored argument name `a` in `G` (i.e., every input of `R`), set `FC.parameter()[a]` from `g.parameter()[a]`:
  - Copy source wiring (ports) if present; else copy value.
- Set `FC.parameter()[FieldNames::StepSize]` from `g.parameter()[FieldNames::StepSize]`.
- Ensure `FC` exposes `FieldNames::Vector` output (it will mirror G’s outputs).
- Rewire all consumers of `g.getOutputs()[FieldNames::Vector]` to `FC.getOutputs()[FieldNames::Vector]`.
- Remove `g` from `M`.

Implementation hint: provide a small utility to rewire consumers of a port to another port in the same model.

## Integration with existing passes

- `OptimizeOutputs`: after lowering, run as usual. Because G calls R via `FunctionCall`, the selected output of R will be marked consumed based on G’s graph, preserving only what’s needed.
- `GraphFlattener`: may inline G, R, or both. Either is fine.
- `ToOCLVisitor`: sees only `FunctionCall` and math nodes now—no special handling needed. The existing `FunctionGradient` code path becomes a defense-only fallback and can be removed later.

## Edge cases and robustness

- Incomplete configuration (`g.hasValidConfiguration()` false):
  - Option A: synthesize a trivial G that returns zero vector and replace `g` (keeps pipeline flowing).
  - Option B: skip replacement and leave `g` (but then codegen must not see it). Prefer A for resilience.
- Missing or invalid `R`: synthesize trivial G as above and replace.
- Step size non-positive: either clamp in UI/node (already done) or insert a `max(abs(h), 1e-8)` guard in graph (e.g., using `Abs`, `Max`).
- Type mismatches: validate mirrored argument types against `R`’s inputs; if mismatch detected due to upstream changes, either retype via replacement nodes or fail the lowering (log error and synthesize zero function).
- Multiple identical gradients: reuse `G` via cache; step size remains a parameter so reuse is safe.
- Cycles: if `R` can (indirectly) call `G` or other gradient functions creating a cycle, detect and abort lowering for that case (extremely unlikely in normal authoring).

## Pseudocode

```cpp
void LowerFunctionGradient::run() {
  auto gradients = collectAllFunctionGradientNodes(assembly);
  Cache cache; // key -> Gid

  for (auto [modelId, gNodeId] : gradients) {
    auto& M = *assembly.findModel(modelId);
    auto& g = dynamic_cast<FunctionGradient&>(*M.findNode(gNodeId));

    // Validate
    if (!g.hasValidConfiguration()) { replaceWithZeroVectorFunction(M, g, cache); continue; }

    auto fid = g.getFunctionId();
    auto R = assembly.findModel(fid);
    if (!R) { replaceWithZeroVectorFunction(M, g, cache); continue; }

    Key key{fid, g.getSelectedScalarOutput(), g.getSelectedVectorInput()};
    ResourceId Gid;
    if (cache.contains(key)) {
      Gid = cache[key];
    } else {
      auto G = synthesizeGradientFunction(*R, key, /*with StepSize input*/ true);
      Gid = assignResourceId(assembly, key);
      G->setResourceId(Gid);
      assembly.insertModel(Gid, std::move(G));
      cache[key] = Gid;
    }

    replaceGradientWithFunctionCall(M, g, Gid);
  }
}
```

Key helpers:
- `collectAllFunctionGradientNodes(Assembly&) -> vector<pair<ModelId, NodeId>>`.
- `synthesizeGradientFunction(Model& R, Key key, bool stepAsInput) -> unique_ptr<Model>`.
- `assignResourceId(Assembly&, Key) -> ResourceId`.
- `replaceGradientWithFunctionCall(Model& M, FunctionGradient& g, ResourceId Gid)`.
- `replaceWithZeroVectorFunction(Model& M, FunctionGradient& g, Cache&)` (creates/reuses trivial function that returns (0,0,0)).
- `rewireConsumers(Model& M, Port& from, Port& to)`.

## Testing plan

- Unit tests:
  - LowersSingleGradient_ProducesFunctionAndReplacesNode
  - ReusesGradientFunction_ForIdenticalConfigs
  - WiresArgumentsCorrectly_MirroredInputsAndStepSize
  - GeneratesExpectedOcl_ForLoweredGraph (asserts central-difference and normalization patterns)
  - IncompleteConfig_LowersToZeroVectorFunction
- Integration smoke:
  - Full pipeline render on a scene containing `FunctionGradient`—no special codegen; visually plausible result.

## Performance considerations

- Six evaluations of `R` per gradient may be inlined by flattener. This can increase code size but improves runtime locality.
- Reuse of G across multiple gradients reduces duplication.
- Future optimizations: fuse symmetric evaluations if upstream graph is cheap to recompute; per-axis adaptive step; optional unnormalized output.

## Rollout

- Implement pass and integrate before `OptimizeOutputs` and `GraphFlattener`.
- Keep `ToOCLVisitor::visit(FunctionGradient)` as a defensive fallback initially; remove once we verify no `FunctionGradient` reaches codegen.
- 3MF: no changes required now; the lowered graph serializes as ordinary nodes. Native `FunctionGradient` support in lib3mf can be added later without blocking.

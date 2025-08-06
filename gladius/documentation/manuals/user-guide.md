# Gladius User Guide: Practical 3MF Modeling with Implicit Geometries

## Table of Contents

1. [Introduction to Gladius](#introduction-to-gladius)
2. [Understanding the Workflow](#understanding-the-workflow)
3. [Key Concepts: Implicits and SDF](#key-concepts-implicits-and-sdf)
4. [Relationship Between Elements](#relationship-between-elements)
5. [Step-by-Step Workflow](#step-by-step-workflow)
6. [Practical Examples](#practical-examples)
7. [Tips and Best Practices](#tips-and-best-practices)
8. [Troubleshooting and FAQs](#troubleshooting-and-faqs)

---

## Introduction to Gladius

Gladius is a cutting-edge tool for creating 3D models using **implicit geometries** and the **3MF format**. It is designed for users who want to create complex, organic, or lattice structures for 3D printing and advanced manufacturing.

### Why Use Gladius?

- **Implicit Modeling**: Define shapes with mathematical precision.
- **3MF Support**: Native compatibility with the 3MF format, including volumetric and implicit extensions.
- **Node-Based Workflow**: Intuitive graph-based interface for creating and combining functions.
- **Real-Time Feedback**: GPU-accelerated rendering for instant visualization.

---

## Understanding the Workflow

In Gladius, the workflow revolves around creating **functions** that define shapes, converting them into **level sets**, and organizing them into **build items** for 3D printing. Here's a high-level overview:

1. **Functions**: Mathematical definitions of shapes (e.g., a box or gyroid lattice).
2. **Level Sets**: Convert functions into 3D objects with defined boundaries.
3. **Resources**: Manage reusable elements like meshes, materials, and functions.
4. **Build Items**: Arrange objects on the build platform for manufacturing.

---

## Key Concepts: Implicits and SDF

### What Are Implicit Geometries?

Implicit geometries use mathematical functions to define shapes. Instead of explicitly modeling surfaces, you define a function `f(x, y, z)` where:

- **f(x, y, z) < 0**: Inside the object
- **f(x, y, z) = 0**: On the surface
- **f(x, y, z) > 0**: Outside the object

This approach allows for infinite precision and easy creation of complex shapes.

### Signed Distance Functions (SDF)

An SDF is a specific type of implicit function that returns the distance to the nearest surface:

- **Positive values**: Outside the object
- **Negative values**: Inside the object
- **Zero**: On the surface

Example: A sphere with radius `r` centered at the origin:
```
f(x, y, z) = sqrt(x² + y² + z²) - r
```

---

## Relationship Between Elements

Understanding the relationship between key elements in Gladius is crucial for mastering the workflow:

1. **Functions**:
   - Define shapes using nodes or mathematical expressions.
   - Can be combined, modified, or reused.

2. **Level Sets**:
   - Represent the 3D geometry of a function.
   - Define boundaries and resolution for manufacturing.

3. **Resources**:
   - Include meshes, materials, and functions.
   - Managed in the Resource Panel.

4. **Build Items**:
   - Arrange level sets and meshes on the build platform.
   - Define transformations (e.g., scaling, rotation).

5. **Volumetric Data**:
   - Add properties like density or color throughout the volume.

---

## Step-by-Step Workflow

### 1. Create a New Project

1. Launch Gladius.
2. Click **"New Project"** on the Welcome Screen.
3. A template job is loaded, which includes:
   - A basic set of predefined functions.
   - A mesh defining a large evaluation domain.

### Note on Level Sets

- A **Level Set** always requires a mesh to define the domain for evaluation.
- The mesh acts as a bounding box, ensuring that implicit functions are evaluated only within the specified region.

### 2. Define Functions

1. **Add a Function**:
   - In the Outline panel, click **"Add function"**.
   - Name your function (e.g., "Box" or "Gyroid").
   - Choose "Empty function" or use the Expression Dialog for complex functions.

2. **Build the Function**:
   - Use the Model Editor to create nodes.
   - Combine 3MF standard elements like **FunctionFromImage3D**, **SignedDistanceToMesh**, and mathematical operations.

### 3. Create Level Sets

1. **Level Set in Template**:
   - The template.3mf already contains a Level Set that references the main function.
   - You can use this existing Level Set or create additional ones as needed.

2. **Add Additional Level Sets** (if needed):
   - In the Outline panel, add a new Level Set.
   - Configure parameters like Min Feature Size and Channel.

### 4. Add Build Items

1. **Build Item in Template**:
   - The template.3mf already contains a Build Item that references the Level Set.
   - This Build Item is ready to use with your functions.

2. **Add Additional Build Items** (if needed):
   - In the Outline panel, click **"Add Build Item"**.
   - Select your Level Set or Mesh from the dropdown.

3. **Arrange on the Build Platform**:
   - Use transformations to position objects.

### 5. Preview and Export

1. **Preview Your Model**:
   - Use the 3D Preview Window to inspect geometry.
   - Adjust parameters in real-time.

2. **Export as 3MF**:
   - Go to **Main Menu** → **Export** → **3MF File**.

---

## Practical Examples

### Example 1: Box with Gyroid Lattice

1. **Create a Box Function**:
   - Use the Expression Dialog to define a box SDF: `length(max(abs(position) - boxSize, 0.0))`
   - Where `boxSize` is a vector defining half-dimensions (e.g., `[1.0, 1.0, 1.0]` for a 2×2×2 box)
   - Alternative: Use math nodes to build the same function step by step

2. **Create a Gyroid Function**:
   - Use the Expression Dialog: `sin(x)cos(y) + sin(y)cos(z) + sin(z)cos(x)`.

3. **Combine Functions**:
   - Use a **Min** node to create a union (combining shapes with distance functions)
   - Connect the Box and Gyroid outputs to the Min node inputs

4. **Use the Existing Level Set**:
   - The template already contains a Level Set that will use your combined function.

5. **Use the Existing Build Item**:
   - The template already contains a Build Item that references the Level Set.
   - Your function will automatically be visible on the build platform.

6. **Export**:
   - Save as a 3MF file.

### Example 2: Custom Lattice Structure

1. **Define a Function**:
   - Use trigonometric nodes to create a lattice pattern.

2. **Preview and Adjust**:
   - Modify parameters to refine the structure.

3. **Export**:
   - Save as 3MF for 3D printing.

---

## Available Nodes and Operations

### Primitive Nodes (3MF Standard)
- **FunctionFromImage3D**: Samples from 3D image data (PNG image stacks)
- **SignedDistanceToMesh**: Uses existing mesh as a signed distance field
- **ImplicitFunction**: Node-based mathematical function graphs

### Math Nodes
- **ConstantScalar**, **ConstantVector**, **ConstantMatrix**: Constants
- **Addition**, **Subtraction**, **Multiplication**, **Division**: Basic operations
- **Min**, **Max**: Essential for boolean operations with distance functions
- **Sine**, **Cosine**, **Tangent**: Trigonometric functions
- **Abs**, **Sqrt**, **Pow**, **Exp**, **Log**: Mathematical functions
- **Length**: Vector magnitude
- **DotProduct**, **CrossProduct**: Vector operations
- **Select**: Conditional selection (if A < B then C else D)
- **Clamp**: Limit values to a range

### Boolean Operations with Distance Functions

Since Gladius works with signed distance functions (SDF), boolean operations are achieved using mathematical nodes:

#### Union (Combining Shapes)
- **Min(shapeA, shapeB)**: Creates a union where both shapes are combined
- The result includes areas where either shape exists

#### Intersection (Common Areas)
- **Max(shapeA, shapeB)**: Creates intersection where both shapes overlap
- Only areas present in both shapes remain

#### Difference (Subtraction)
- **Max(shapeA, -shapeB)**: Subtracts shapeB from shapeA
- Use **Subtraction** node to negate shapeB before applying Max

#### Smooth Transitions
- You can build custom functions to create smooth transitions between shapes
- The template.3mf file contains examples of smooth transition functions
- Use mathematical combinations of existing operations to achieve organic blending

---

## Tips and Best Practices

1. **Start Simple**: Begin with basic shapes and gradually add complexity.
2. **Use the Expression Dialog**: Simplify complex functions with mathematical expressions.
3. **Preview Frequently**: Check your model in the 3D Preview Window.
4. **Save Incrementally**: Keep multiple versions of your project.
5. **Leverage Examples**: Use the Library Browser to explore pre-built models.

---

## Troubleshooting and FAQs

### Common Issues

1. **Function Errors**:
   - Check node connections and data types.
   - Ensure all inputs are defined.

2. **Performance Problems**:
   - Increase Min Feature Size.
   - Simplify node graphs.

3. **Export Failures**:
   - Validate 3MF compliance.
   - Test with simple models first.

### FAQs

1. **Can I combine implicit and mesh objects?**
   - Yes, use the Resource Panel to manage both.

2. **How do I create smooth transitions?**
   - Build custom functions using mathematical combinations of basic operations.
   - Refer to template.3mf for examples of smooth transition implementations.

3. **What file formats are supported?**
   - Gladius supports 3MF natively and can import/export STL and OBJ.

---

*This guide is designed to help you get started with Gladius and the 3MF format. For advanced topics, refer to the official documentation.*

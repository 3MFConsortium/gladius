
# Plan: MCP Tools for Direct Graph Manipulation

**Author:** Gemini
**Date:** 2025-08-29

## 1. Overview

This document outlines a plan to extend the Gladius MCP (Model-Context-Protocol) server with a new set of tools for the direct, programmatic manipulation of function graphs. Currently, functions can be created from mathematical expressions, but there is no MCP-level interface for modifying the underlying graph structure.

These new tools will enable AI agents and developers to dynamically build, modify, and inspect function graphs by creating and deleting nodes, and establishing connections (links) between them. This will unlock more advanced generative design workflows and allow for finer-grained control over the implicit modeling process.

## 2. Target Audience

*   **AI Agents:** Language models and other AI systems that will use these tools to autonomously create and optimize implicit models.
*   **Developers:** Engineers building tools and integrations on top of the Gladius platform.

## 3. Core Concepts

The new tools will operate on the following core concepts:

*   **Function ID (`function_id`):** All graph manipulations are performed within the context of a specific function. This is identified by the `ResourceId` of the `Model` resource.
*   **Node ID (`node_id`):** Every node within a function graph is uniquely identified by a `NodeId`. This ID is used to target specific nodes for modification or deletion.
*   **Node Type (`node_type`):** This is a string representing the C++ class name of the node to be created (e.g., `Addition`, `ConstantScalar`, `FunctionCall`).
*   **Ports and Parameters:** Nodes have input sockets (called **Parameters**) and output sockets (called **Ports**). Links are formed by connecting a source **Port** to a target **Parameter**. Both are identified by their string names (e.g., `A`, `B`, `Result`, `Value`).

## 4. Proposed MCP Tools

The following tools will be added to the MCP server.

### 4.1. `create_node`

Creates a new node within a specified function graph.

*   **Description:** Adds a new node of a given type to the function graph.
*   **Arguments:**
    *   `function_id` (integer, required): The `ResourceId` of the function to add the node to.
    *   `node_type` (string, required): The class name of the node to create (e.g., "Addition", "Sine", "ConstantScalar", "FunctionCall").
    *   `display_name` (string, optional): A user-friendly name for the node. If not provided, a default name will be generated.
    *   `node_id` (integer, optional): A specific ID for the new node. If omitted, a new unique ID will be automatically generated.
*   **Returns:**
    *   `node_id` (integer): The ID of the newly created node.
    *   `success` (boolean): Indicates if the operation was successful.
    *   `error` (string, optional): An error message if the operation failed.

### 4.2. `delete_node`

Deletes a node from a function graph.

*   **Description:** Removes a specified node from the function graph. All links to and from the node will be automatically disconnected.
*   **Arguments:**
    *   `function_id` (integer, required): The `ResourceId` of the function containing the node.
    *   `node_id` (integer, required): The ID of the node to delete.
*   **Returns:**
    *   `success` (boolean): Indicates if the operation was successful.
    *   `error` (string, optional): An error message if the operation failed.

### 4.3. `create_link`

Connects the output port of one node to the input parameter of another.

*   **Description:** Creates a directed link from a source node's output to a target node's input.
*   **Arguments:**
    *   `function_id` (integer, required): The `ResourceId` of the function where the link will be created.
    *   `source_node_id` (integer, required): The `NodeId` of the node providing the output.
    *   `source_port_name` (string, required): The name of the output port on the source node (e.g., "Result", "Value").
    *   `target_node_id` (integer, required): The `NodeId` of the node receiving the input.
    *   `target_parameter_name` (string, required): The name of the input parameter on the target node (e.g., "A", "B").
*   **Returns:**
    *   `success` (boolean): Indicates if the operation was successful.
    *   `error` (string, optional): An error message if the operation failed (e.g., type mismatch).

### 4.4. `delete_link`

Removes a connection to a node's input parameter.

*   **Description:** Disconnects the link feeding into a specific input parameter.
*   **Arguments:**
    *   `function_id` (integer, required): The `ResourceId` of the function containing the link.
    *   `target_node_id` (integer, required): The `NodeId` of the node with the input parameter to be disconnected.
    *   `target_parameter_name` (string, required): The name of the input parameter to disconnect.
*   **Returns:**
    *   `success` (boolean): Indicates if the operation was successful.
    *   `error` (string, optional): An error message if the operation failed.

### 4.5. `set_parameter_value`

Sets the literal value of a node's input parameter, assuming it is not connected to another node's output.

*   **Description:** Assigns a constant value to a parameter. This is only possible if the parameter is not driven by a link from another node.
*   **Arguments:**
    *   `function_id` (integer, required): The `ResourceId` of the function containing the node.
    *   `node_id` (integer, required): The `NodeId` of the node to modify.
    *   `parameter_name` (string, required): The name of the parameter to set.
    *   `value` (variant, required): The value to set. The type must be compatible with the parameter's expected type (e.g., `float`, `float3`, `string`, `integer`).
*   **Returns:**
    *   `success` (boolean): Indicates if the operation was successful.
    *   `error` (string, optional): An error message if the operation failed.

### 4.6. `get_node_info`

Retrieves detailed information about a specific node.

*   **Description:** Inspects a node and returns its properties, including its parameters and output ports.
*   **Arguments:**
    *   `function_id` (integer, required): The `ResourceId` of the function containing the node.
    *   `node_id` (integer, required): The `NodeId` of the node to inspect.
*   **Returns:**
    *   A JSON object containing:
        *   `node_id` (integer)
        *   `node_type` (string)
        *   `display_name` (string)
        *   `parameters` (object): A map of parameter names to their details, including `type`, `value` (if not linked), and `source` (if linked).
        *   `outputs` (object): A map of output port names to their details, including `type`.
    *   `success` (boolean): Indicates if the operation was successful.
    *   `error` (string, optional): An error message if the operation failed.

## 5. Example Workflow

Here is an example of how an AI agent could use these tools to create a simple function that adds two numbers (5 and 10).

1.  **Create a new function:**
    *   Use the existing `create_function_from_expression` tool with a dummy expression to get a new `function_id`.
    ```json
    { "name": "MyAdder", "expression": "0" }
    ```
    *   Let's assume the returned `function_id` is `123`.

2.  **Create two `ConstantScalar` nodes:**
    *   `mcp_tool.create_node(function_id=123, node_type="ConstantScalar", display_name="Five")` -> returns `node_id: 1`
    *   `mcp_tool.create_node(function_id=123, node_type="ConstantScalar", display_name="Ten")` -> returns `node_id: 2`

3.  **Set the values of the constant nodes:**
    *   `mcp_tool.set_parameter_value(function_id=123, node_id=1, parameter_name="Value", value=5.0)`
    *   `mcp_tool.set_parameter_value(function_id=123, node_id=2, parameter_name="Value", value=10.0)`

4.  **Create an `Addition` node:**
    *   `mcp_tool.create_node(function_id=123, node_type="Addition", display_name="Add")` -> returns `node_id: 3`

5.  **Create the links:**
    *   Connect the first constant to the "A" input of the addition node:
        `mcp_tool.create_link(function_id=123, source_node_id=1, source_port_name="Value", target_node_id=3, target_parameter_name="A")`
    *   Connect the second constant to the "B" input of the addition node:
        `mcp_tool.create_link(function_id=123, source_node_id=2, source_port_name="Value", target_node_id=3, target_parameter_name="B")`

6.  **Connect the result to the function's output:**
    *   The function graph has a special "Output" node (an `End` node). We need to find its ID first (e.g., by iterating through nodes or having a dedicated tool). Let's assume its ID is `0`.
    *   `mcp_tool.create_link(function_id=123, source_node_id=3, source_port_name="Result", target_node_id=0, target_parameter_name="Shape")`

## 6. Error Handling

The implementation of these tools must include robust error handling to ensure graph integrity.

*   **Invalid IDs:** If a `function_id` or `node_id` does not exist, the tool should return a "Not Found" error.
*   **Type Mismatches:** `create_link` must validate that the source port's type is compatible with the target parameter's type. If not, it should return an error.
*   **Invalid Node Types:** `create_node` should return an error if the requested `node_type` does not exist.
*   **Dangling Links:** `delete_node` should ensure that all links connected to the deleted node are properly removed.
*   **Parameter Value Errors:** `set_parameter_value` should return an error if the parameter is already connected to a link or if the value's type is incorrect.

By implementing this comprehensive set of tools, we can significantly enhance the capabilities of the MCP server and open up new possibilities for advanced, AI-driven 3D modeling in Gladius.

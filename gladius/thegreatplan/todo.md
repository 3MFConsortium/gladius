
# Feature wish list

Consider also adding tests for the features you implement.

## ToDo
1. add more keyboard short cuts: compile implicit function, center view, top view, front view, left view, right view, back view, bottom view, zoom in, zoom out, reset zoom, auto layout, create node
2. add copy/paste functionality to the node editor for nodes and connections
3. add a way to extract a selection of nodes and connections to a new function
4. allow parts to be rendered outside of the build volume
5. improve the backup workflow
    - add a way to restore from a backup
    - add a session ID to the backup file name, so that it doesn't get overwritten
    - offer to restore the backup in the welcome dialog
6. function parser to parse the function code and create the nodes and connections automatically
7. dialog to add a function from a mathematical expression
8. text based function editor, allow switching between text and node editor
9. gsls import
10. gsls export (e.g. shadertoy)

## Done

1. automatic compilation ("Compile automatically" Button in modeleditor) of the implicit model doesn't work currently, fix it - COMPLETED: Fixed the issue where `markModelAsUpToDate()` was called before checking `isCompileRequested()`, preventing automatic compilation from working properly.
2. node editor: consider grouping of nodes in the auto layout - COMPLETED: Implemented group-aware node layout with proper topological ordering both between and within groups, moved layout algorithm to separate NodeLayoutEngine class.

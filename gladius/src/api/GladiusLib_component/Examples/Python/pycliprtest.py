import matplotlib.pyplot as plt
import numpy as np

import pyclipr

# Tuple definition of a path
path = np.array([(0.0, 0.), (0, 105.1234), (100, 105.1234), (100, 0), (0, 0)])
path2 = np.array([(10.0, 10.0), (5.0, 50), (50.0, 50), (50., 10.0), (10.0, 10.0)])
path2 = np.flipud(path2)

path3 = np.array([(-10.0, -10.0), (70.0, -10), (70.0, 70), (-10., 70.0), (-10.0, -10.0)])

"""
Create an offsetting PyClipr tool
"""
po = pyclipr.ClipperOffset()

# Set the scale factor to convert to internal integer representation
# Note the default is set to 1000 units
po.scaleFactor = int(1e5)

# add the path - ensuring to use Polygon for the endType argument
po.addPaths([path, path2], pyclipr.JoinType.Miter, pyclipr.EndType.Polygon)
"""
Apply the offsetting operation using a delta/offset
NoteL this automatically scaled interally scaled within pyclipr.ClipperOffset
"""
offsetOut = po.execute(2.0)

# Plot the original and offset polygons using Matplotlib
plt.figure()
plt.axis("equal")
plt.title("Polygon Offset using Matplotlib")
plt.xlabel("X Axis")
plt.ylabel("Y Axis")

# Plot original paths in blue with a light fill
for p in [path, path2]:
    plt.plot(p[:, 0], p[:, 1], "b-", lw=2, label="Original" if "Original" not in plt.gca().get_legend_handles_labels()[1] else "")
    plt.fill(p[:, 0], p[:, 1], facecolor="blue", alpha=0.1)

# Plot offset paths in red dashed lines with a light fill
for poly in offsetOut:
    plt.plot(poly[:, 0], poly[:, 1], "r--", lw=2, label="Offset" if "Offset" not in plt.gca().get_legend_handles_labels()[1] else "")
    plt.fill(poly[:, 0], poly[:, 1], facecolor="red", alpha=0.1)

plt.legend(loc="best")
plt.show()


"""
Create a Pyclipr clipping tool
"""
pc = pyclipr.Clipper()
pc.scaleFactor = int(1000)

"""
Add the paths to the clipping object. Ensure the subject and clip arguments are set to differentiate
the paths during the Boolean operation. The final argument specifies if the path is open.
"""
pc.addPaths(offsetOut, pyclipr.Subject)

# Note it is possible to add a path as a numpy array (nx2) or as a list of tuples
pc.addPath(np.array(path3), pyclipr.Clip)

"""
Perform Polygon Clipping 
The fill rule is optional and by defualt in ClipperLib is set to EvenOdd
"""

out = pc.execute(pyclipr.Intersection)
out2 = pc.execute(pyclipr.Union, pyclipr.FillRule.EvenOdd)
out3 = pc.execute(pyclipr.Difference, pyclipr.FillRule.EvenOdd)
out4 = pc.execute(pyclipr.Xor, pyclipr.FillRule.EvenOdd)

"""
Returning PolyTree structures following clipping operations:
Using execute2 returns a PolyTree structure that provides hierarchical information
"""
# if the paths are interior or exterior
outB = pc.execute2(pyclipr.Intersection)

# An alternative equivalent name is executeTree
outB = pc.executeTree(pyclipr.Intersection)

"""
Test Open Path Clipping:
# Pyclipr can be used for clipping open paths. This has become simple to complete using the Clipper2 library
"""

# Create a new Clipper Object
pc2 = pyclipr.Clipper()
pc2.scaleFactor = int(1e5)

"""
The open path is added as a subject (note the final argument 'isOpen' set to True). By default all paths are 
assumed to be closed in pyclipr
"""
pc2.addPath(((40, -10), (50, 130)), pyclipr.Subject, True)

# The clipping object is usually set to the Polygon
pc2.addPaths(offsetOut, pyclipr.Clip, False)

""" 
Test the return types for open path clipping with option enabled. When The returnOpenPaths argument is set to True
this will return the open paths. Note this function only works well using the Boolean intersection option
"""
outC = pc2.execute(pyclipr.Intersection, pyclipr.FillRule.NonZero)
outC2, openPathsC = pc2.execute(pyclipr.Intersection, pyclipr.FillRule.NonZero, returnOpenPaths=True)

outD = pc2.execute2(pyclipr.Intersection, pyclipr.FillRule.NonZero)
outD2, openPathsD = pc2.execute2(pyclipr.Intersection, pyclipr.FillRule.NonZero, returnOpenPaths=True)

"""
Plot the results from the examples
"""
pathPoly = np.array(path)

plt.figure()
plt.axis('equal')

# Plot the original polygon
plt.fill(pathPoly[:, 0], pathPoly[:, 1], 'b', alpha=0.1, linewidth=1.0, linestyle='dashed', edgecolor='#000')

# Plot the offset square
plt.fill(offsetOut[0][:, 0], offsetOut[0][:, 1],
         linewidth=1.0, linestyle='dashed', edgecolor='#333', facecolor='none')

# Plot the intersection
plt.fill(out[0][:, 0], out[0][:, 1], facecolor='#75507b')

# Plot the open path intersection
plt.plot(openPathsC[0][:, 0], openPathsC[0][:, 1],
         color='#222', linewidth=1.0, linestyle='dashed', marker='.', markersize=20.0)
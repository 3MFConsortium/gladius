'''++

Copyright (C) 2021 Jan Orend

All rights reserved.

This file has been generated by the Automatic Component Toolkit (ACT) version 1.6.0.

Abstract: This is an autogenerated Python application that demonstrates the
 usage of the Python bindings of GladiusLib

Interface version: 1.0.0

'''


import os
import sys
sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", "..", "Bindings", "Python"))
import GladiusLib


def main():
	cwd = os.getcwd()
	origDir = cwd
	os.chdir(cwd + "/../../Implementations/Cpp")
	cwd = os.getcwd()
	print("Current working directory: {0}".format(cwd))
	libpath = cwd
	wrapper = GladiusLib.Wrapper(libraryName = os.path.join(libpath, "gladiuslib"))
	
	major, minor, micro = wrapper.GetVersion()
	print("GladiusLib version: {:d}.{:d}.{:d}".format(major, minor, micro), end="")
	print("")

	gladius = wrapper.CreateGladius()
	gladius.LoadAssembly("D:/gladiusModels/gravitrax/spacer.json")
	
	for height_mm in [20, 40, 80]:
		gladius.SetFloatParameter("Assembly", "Part_5", "part_height", height_mm)
		filename = origDir + "/spacer{:1.1f}_mm.stl".format(height_mm)
		gladius.ExportSTL(filename)
		#z_height = 10.0
		#contour = gladius.GenerateContour(z_height, 0.0)
		#print("Number of polygons in contour: {}".format(contour.GetSize()))



if __name__ == "__main__":
	try:
		main()
	except GladiusLib.EGladiusLibException as e:
		print(e)

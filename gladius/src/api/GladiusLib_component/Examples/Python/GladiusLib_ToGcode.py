import math
import os
import sys
import wx
import pyclipr
import numpy as np
from ortools.constraint_solver import pywrapcp, routing_enums_pb2
import time
from concurrent.futures import ThreadPoolExecutor


sys.path.append(
    os.path.join(
        os.path.dirname(os.path.realpath(__file__)), "..", "..", "Bindings", "Python"
    )
)
import GladiusLib


class SlicingParameters:

    def __init__(
        self,
        first_layer_height,
        layer_height,
        num_perimeters = 10,
        volume_rate_mm3_per_s = 10.0,
        extrusion_width=0.8,
        nozzle_diameter=0.6,
        nozzle_temperature=200,
        travel_speed=250,
        perimeter_overlap=0.15,
        fan_speed=255,
        acceleration=5000,
        brim_width=5.0,
    ):
        self.first_layer_height = first_layer_height  # mm
        self.layer_height = layer_height  # mm
        self.num_perimeters = num_perimeters
        self.volume_rate_mm3_per_s = volume_rate_mm3_per_s  # mm^3/s
        self.extrusion_width = extrusion_width  # mm
        self.nozzle_diameter = nozzle_diameter  # mm
        self.nozzle_temperature = nozzle_temperature  # C
        self.travel_speed = travel_speed  # mm/s
        self.perimeter_overlap = perimeter_overlap
        self.fan_speed = fan_speed
        self.acceleration = acceleration  # mm/s^2
        self.brim_width = brim_width  # mm


def select_file(default_dir):
    dialog = wx.FileDialog(
        None,
        "Select 3MF file",
        wildcard="*.3mf",
        style=wx.FD_OPEN | wx.FD_FILE_MUST_EXIST,
        defaultDir=default_dir,
    )
    if dialog.ShowModal() == wx.ID_CANCEL:
        return None
    return dialog.GetPath()


def select_gcode_start(default_dir):
    dialog = wx.FileDialog(
        None,
        "Select start gcode for your printer",
        wildcard="*.gcode",
        style=wx.FD_OPEN | wx.FD_FILE_MUST_EXIST,
        defaultDir=default_dir,
    )
    if dialog.ShowModal() == wx.ID_CANCEL:
        return None
    return dialog.GetPath()


def save_file(default_dir, default_filename):
    dialog = wx.FileDialog(
        None,
        "Save G-code file",
        wildcard="*.gcode",
        style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT,
        defaultDir=default_dir,
        defaultFile=default_filename,
    )
    if dialog.ShowModal() == wx.ID_CANCEL:
        return None
    return dialog.GetPath()


class GcodeWriter:
    def __init__(self, file, slicing_params):
        self.file = file
        self.slicing_params = slicing_params
        self.extruder_position = 0.0  # Initialize the extruder position, mm

    def enableAbsolutePositioning(self):
        self.file.write("\nG90\n")
        # also force the extruder to be in absolute mode
        self.file.write("M82\n")

    def setExtruderTemperature(self):
        self.file.write(
            f"M109 S{self.slicing_params.nozzle_temperature}\
            ; wait for nozzle temperature to be reached\n"
        )

    def setExtruderTemperatureWithoutWait(self):
        self.file.write(f"M104 S{self.slicing_params.nozzle_temperature}\n")

    # Set the fan speed in the range 0-255
    def setFanSpeed(self, speed):
        self.file.write(f"M106 S{speed}\n")

    def setAcceleration(self):
        self.file.write(
            f"M204 S{self.slicing_params.acceleration}\n"
        )

    def layerChange(self, z_height, layer_height):
        self.file.write(f";LAYER_CHANGE\n")
        self.file.write(f";Z:{z_height:.2f}\n")
        self.file.write(f";HEIGHT:{layer_height:.2f}\n")
        self.file.write(f";BEFORE_LAYER_CHANGE\n")
        self.file.write(f";{z_height:.2f}\n")

        self.file.write(f"G1 Z{z_height:.2f}\n")

        # reset the extruder position
        self.extruder_position = 0.0
        self.file.write(f"G92 E0\n")

    def addPolygons(self, polygons, layer_height):
        track_area_mm2 = (
            self.slicing_params.extrusion_width - layer_height
        ) * layer_height + layer_height * layer_height * 0.25 * np.pi

        feedrate_mm_per_s = self.slicing_params.volume_rate_mm3_per_s / track_area_mm2
        feedrate_mm_per_min = feedrate_mm_per_s * 60.0

        nozzle_area = np.pi * (self.slicing_params.nozzle_diameter * 0.5) ** 2
        extrusion_per_extruded_mm = track_area_mm2 / nozzle_area * 0.1

        for polygon in polygons:
            if len(polygon) == 0:
                continue
            previous_vertex = polygon[0]
            # Move to the first vertex of the polygon without extruding
            travelSpeed_mm_per_min = self.slicing_params.travel_speed * 60.0
            self.file.write(
                f"G1 X{previous_vertex[0]:.4f} Y{previous_vertex[1]:.4f} F{travelSpeed_mm_per_min:.4f}\n"
            )

            self.file.write(f";TYPE:Perimeter\n")
            self.file.write(f";WIDTH:{self.slicing_params.extrusion_width:.6f}\n")

            # just for debugging
            # extrusion_per_extruded_mm

            self.file.write(f";EXTRUSION:{extrusion_per_extruded_mm:.6f}\n")
            self.file.write(f";FEEDRATE:{feedrate_mm_per_min:.4f}\n")
            self.file.write(f";nozzle_area:{nozzle_area:.6f}\n")
            self.file.write(f";track_area_mm2:{track_area_mm2:.6f}\n")

            # Write feedrate
            self.file.write(f"G1 F{feedrate_mm_per_min:.4f}\n")
            # Add the first vertex to the end of the polygon to close it
            polygon = np.vstack([polygon, polygon[0]])

            for vertex in polygon:
                currentVertex = vertex
                # Calculate the distance between the previous vertex and the current vertex
                distance = math.sqrt(
                    (currentVertex[0] - previous_vertex[0]) ** 2
                    + (currentVertex[1] - previous_vertex[1]) ** 2
                )
                extruder_distance = distance * extrusion_per_extruded_mm

                self.extruder_position += extruder_distance

                self.file.write(
                    f"G1 X{currentVertex[0]:.4f} Y{currentVertex[1]:.4f} E{self.extruder_position:.5f}\n"
                )
                previous_vertex = currentVertex

    def endPrint(self):
        self.file.write("M104 S0\n")
        self.file.write("M140 S0\n")
        self.file.write("END_PRINT\n")


def process_polygon(polygon):
    vertices = []
    while polygon.GetSize() > 0:
        vertex = polygon.GetCurrentVertex()
        vertices.append((vertex.Coordinates[0], vertex.Coordinates[1]))
        if not polygon.Next():
            break
    # Close the polygon
    vertices.append(vertices[0])
    return vertices


def process_contour(contour):
    polygons = []
    while contour.GetSize() > 0:
        polygon = contour.GetCurrentPolygon()
        vertices = process_polygon(polygon)

        polygons.append(vertices)
        if not contour.Next():
            break
    return polygons


def reduce_travel_moves(polygons):
    # Extract the first vertex of each polygon
    vertices = [polygon[0] for polygon in polygons]

    # Compute the distance matrix
    num_vertices = len(vertices)
    if num_vertices == 0:
        return polygons

    # Convert vertices to a NumPy array
    vertices_array = np.array(vertices)

    # Compute the distance matrix using broadcasting
    diff = vertices_array[:, np.newaxis, :] - vertices_array[np.newaxis, :, :]
    squared_diff = np.sum(diff**2, axis=-1)
    distance_matrix = np.round(np.sqrt(squared_diff) * 1e4).astype(int)

    # Create the routing index manager
    manager = pywrapcp.RoutingIndexManager(num_vertices, 1, 0)

    # Create Routing Model
    routing = pywrapcp.RoutingModel(manager)

    def distance_callback(from_index, to_index):
        from_node = manager.IndexToNode(from_index)
        to_node = manager.IndexToNode(to_index)
        return round(distance_matrix[from_node][to_node])

    transit_callback_index = routing.RegisterTransitCallback(distance_callback)
    routing.SetArcCostEvaluatorOfAllVehicles(transit_callback_index)

    # Allow to start and end at arbitrary locations
    for node in range(num_vertices):
        routing.AddVariableMinimizedByFinalizer(routing.NextVar(node))

    # Set parameters
    search_parameters = pywrapcp.DefaultRoutingSearchParameters()
    search_parameters.first_solution_strategy = (
        routing_enums_pb2.FirstSolutionStrategy.PATH_CHEAPEST_ARC
    )

    search_parameters.time_limit.seconds = 1

    # Solve the problem
    solution = routing.SolveWithParameters(search_parameters)

    # Extract the solution
    if solution:
        index = routing.Start(0)
        permutation = []
        while not routing.IsEnd(index):
            permutation.append(manager.IndexToNode(index))
            index = solution.Value(routing.NextVar(index))

        # Reorder the polygons according to the TSP solution
        ordered_polygons = [polygons[i] for i in permutation]

        return ordered_polygons
    else:
        print("No solution found!")
        return polygons


def main():
    app = wx.App(False)
    scriptpath = os.path.dirname(os.path.realpath(__file__))
    wrapper = GladiusLib.Wrapper(libraryName=os.path.join(scriptpath, "gladiuslib"))

    major, minor, micro = wrapper.GetVersion()
    print("GladiusLib version: {:d}.{:d}.{:d}".format(major, minor, micro), end="")
    print("")

    gladius = wrapper.CreateGladius()
    input_file = select_file("/home/jan/projects/gadgets/slicingtest")
    if not input_file:
        print("No file selected.")
        return

    gcode_start = select_gcode_start(os.path.join(scriptpath, "profiles"))
    if not gcode_start:
        print("No start gcode selected.")
        return

    default_dir = os.path.dirname(input_file)
    default_filename = os.path.splitext(os.path.basename(input_file))[0] + ".gcode"
    output_file = save_file(default_dir, default_filename)
    if not output_file:
        print("No output file selected.")
        return

    slicing_params = SlicingParameters(
        first_layer_height=0.3,
        layer_height=0.1,
        num_perimeters=50,
        volume_rate_mm3_per_s=10.0,
        extrusion_width=0.7,
        nozzle_diameter=0.6,
        nozzle_temperature=200,
        travel_speed=250,
        perimeter_overlap=0.15,
        brim_width=0.0
    )

    gladius.LoadAssembly(input_file)

    bounding_box = gladius.ComputeBoundingBox()
    print(
        "Bounding box: [{:.2f}, {:.2f}, {:.2f}] x [{:.2f}, {:.2f}, {:.2f}]".format(
            bounding_box.GetMin().Coordinates[0],
            bounding_box.GetMin().Coordinates[1],
            bounding_box.GetMin().Coordinates[2],
            bounding_box.GetMax().Coordinates[0],
            bounding_box.GetMax().Coordinates[1],
            bounding_box.GetMax().Coordinates[2],
        )
    )

    # Generate contours at slicing_params.layer_height intervals, starting from the bottom of the bounding box, moveing everything down to z=0
    z_min = bounding_box.GetMin().Coordinates[2]
    z_max = bounding_box.GetMax().Coordinates[2] - z_min
    z_step = slicing_params.layer_height

    z_range = range(0, int(z_max * 1000), int(z_step * 1000))
    total_steps = len(z_range)

    progress_dialog = wx.ProgressDialog(
        "Generating Contours",
        "Progress",
        maximum=total_steps,
        parent=None,
        style=wx.PD_AUTO_HIDE | wx.PD_APP_MODAL,
    )

    previous_z = 0.0
    with open(output_file, "w") as f:
        with open(gcode_start, "r") as start_file:
            f.write(start_file.read())

        gcode_writer = GcodeWriter(f, slicing_params)
        gcode_writer.enableAbsolutePositioning()

        gcode_writer.setAcceleration()
        gcode_writer.setExtruderTemperatureWithoutWait()

        for i, z_height in enumerate(z_range):
            z_height_mm = slicing_params.first_layer_height + z_height / 1000.0
            z_height_in_model = z_height_mm + z_min
            layer_height = z_height_mm - previous_z
            previous_z = z_height_mm

            gcode_writer.layerChange(z_height_mm, layer_height)
            gcode_writer.setAcceleration()

            offset = -slicing_params.extrusion_width * 0.5
            contour = gladius.GenerateContour(z_height_in_model, offset)
            # Convert contour to pyclipr format
            polygons = process_contour(contour)
            sorted_perimeter = reduce_travel_moves(polygons)
            gcode_writer.addPolygons(sorted_perimeter, layer_height)

            polygons = []

            # loop through the requested perimeters
            offsetRange = range(0, slicing_params.num_perimeters)

            # Add brim for the first layer additional to the perimeters
            # brim_width = slicing_params.brim_width
            # extend the offset range by the number of brim lines
            if i == 0:
                num_brim_lines = int(math.ceil(slicing_params.brim_width / slicing_params.extrusion_width))
                offsetRange = range(-num_brim_lines, slicing_params.num_perimeters)


            for j in offsetRange:
                contour = gladius.GenerateContour(z_height_in_model, offset)
                # Convert contour to pyclipr format and append to polygons
                polygons.extend(process_contour(contour))
                offset -= (math.copysign(1, j) * slicing_params.extrusion_width * (1.0 - slicing_params.perimeter_overlap))
                
            progress_dialog.Update(
                i + 1, f"Generating contours at Z={z_height_mm:.2f} mm"
            )

            sorted_perimeter = reduce_travel_moves(polygons)

            gcode_writer.addPolygons(sorted_perimeter, layer_height)

            print(
                "Contours at Z={:.2f} mm: Number of polygons in contour: {}".format(
                    z_height_in_model, contour.GetSize()
                )
            )
          
            gcode_writer.setFanSpeed(slicing_params.fan_speed)
        gcode_writer.endPrint()
    progress_dialog.Destroy()


if __name__ == "__main__":
    try:
        main()
    except GladiusLib.EGladiusLibException as e:
        print(e)

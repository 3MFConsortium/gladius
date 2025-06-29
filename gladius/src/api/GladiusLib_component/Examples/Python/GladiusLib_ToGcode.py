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
        first_layer_extrusion_width=0.6,
        first_layer_temperature=220,
        layer_height=0.1,
        num_perimeters=10,
        volume_rate_mm3_per_s=10.0,
        extrusion_width=0.8,
        nozzle_diameter=0.6,
        nozzle_temperature=200,
        travel_speed=250,
        perimeter_overlap=0.15,
        fan_speed=255,
        acceleration=5000,
        brim_width=5.0,
        bed_temperature=60,
        extrusion_multiplier=1.0,
        # Infill parameters
        infill_density=0.2,  # 0.0 to 1.0, where 0.2 means 20% infill
        infill_angle=45,     # Degrees - direction of infill lines
        infill_overlap=0.15, # Overlap with perimeters
        infill_layer_freq=1  # Apply infill on every nth layer (1 = every layer)
    ):
        self.first_layer_height = first_layer_height  # mm
        self.first_layer_extrusion_width = first_layer_extrusion_width  # mm
        self.first_layer_temperature = first_layer_temperature  # C
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
        self.bed_temperature = bed_temperature
        self.extrusion_multiplier = extrusion_multiplier  # 1.0 = 100% extrusion
        # Infill parameters
        self.infill_density = infill_density
        self.infill_angle = infill_angle
        self.infill_overlap = infill_overlap
        self.infill_layer_freq = infill_layer_freq


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

    def setExtruderTemperature(self, temperature):
        self.file.write(
            f"M109 S{temperature}\
            ; wait for nozzle temperature to be reached\n"
        )

    def setExtruderTemperatureWithoutWait(self, temperature):
        self.file.write(f"M104 S{temperature}\n")

    # Set the fan speed in the range 0-255
    def setFanSpeed(self, speed):
        self.file.write(f"M106 S{speed}\n")

    def setAcceleration(self):
        self.file.write(f"M204 S{self.slicing_params.acceleration}\n")

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

        feedrate_mm_per_s = self.slicing_params.extrusion_multiplier * self.slicing_params.volume_rate_mm3_per_s / track_area_mm2
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

    def addPause(self):
        self.file.write("PAUSE\n")

    def setBedTemperature(self):
        self.file.write(
            f"M190 S{self.slicing_params.bed_temperature}\
            ; wait for bed temperature to be reached\n"
        )

    def endPrint(self):
        self.file.write("M104 S0\n")
        self.file.write("M140 S0\n")
        self.file.write("END_PRINT\n")

    def startPrint(self):
        self.file.write("M73 P0 R30\n")
        self.file.write(
            f"M190 S{self.slicing_params.bed_temperature} ; set bed temperature and wait for it to be reached\n"
        )
        self.file.write(
            f"M104 S{self.slicing_params.nozzle_temperature} ; set temperature\n"
        )
        self.file.write(";TYPE:Custom\n")
        self.file.write("SET_FAN_SPEED FAN=nevermore1 SPEED=1\n")
        self.file.write("SET_FAN_SPEED FAN=nevermore2 SPEED=1\n")
        self.file.write("MMU__MOTORS_OFF\n")
        self.file.write("MMU ENABLE=0\n")
        self.file.write(
            f"START_PRINT REFERENCED_TOOLS=!referenced_tools! INITIAL_TOOL=0 EXTRUDER_TEMP={self.slicing_params.nozzle_temperature} BED_TEMP={self.slicing_params.bed_temperature}\n"
        )
        self.file.write(";SET_SKEW XY=AC,BD,AD\n")
        self.file.write("SET_SKEW XY=140.8,141.0,99.7\n")
        self.file.write("MMU__MOTORS_OFF\n")
        self.file.write("MMU ENABLE=0\n")
        self.file.write(
            f"M109 S{self.slicing_params.nozzle_temperature} ; set temperature and wait for it to be reached\n"
        )
        self.file.write("G21 ; set units to millimeters\n")
        self.file.write("G90 ; use absolute coordinates\n")
        self.file.write("M83 ; use relative distances for extrusion\n")
        self.file.write("; Filament gcode\n")
        self.file.write("SET_GCODE_OFFSET Z=0\n")
        self.file.write("SET_PRESSURE_ADVANCE ADVANCE=0.00\n")
        self.file.write("SET_PRESSURE_ADVANCE EXTRUDER=extruder SMOOTH_TIME=0.02\n")
        self.file.write("M107\n")

    def startObject(self, object_name):
        self.file.write(f";[exclude_object {object_name}]\n")

    def endObject(self):
        self.file.write(";[end_exclude_object]\n")

    def setSpeedFactor(self, speed_factor):
        self.file.write(f"M220 S{speed_factor}\n")


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


def generate_straight_line_infill(polygons, line_spacing, angle, bounds):
    """
    Generate a straight line infill pattern for the given polygon.
    
    Args:
        polygons: List of polygons in pyclipr format (list of (x,y) coordinate tuples)
        line_spacing: Distance between infill lines in mm
        angle: Angle of the infill lines in degrees
        bounds: Bounding box of the slice [min_x, min_y, max_x, max_y]
        
    Returns:
        List of infill line segments as pairs of points [(x1,y1,x2,y2)]
    """
    # Convert to pyclipr format - using numpy arrays
    clipper_polygons = []
    for polygon in polygons:
        # Skip empty polygons
        if len(polygon) < 3:
            continue
        # Convert to numpy array for pyclipr
        clipper_polygon = np.array(polygon)
        clipper_polygons.append(clipper_polygon)
    
    # Skip if no valid polygons
    if not clipper_polygons:
        return []
    
    # Create clipper object
    clipper = pyclipr.Clipper()
    clipper.scaleFactor = int(1000)  # Set scale factor as in the example
    
    # Add all polygons to clipper
    for polygon in clipper_polygons:
        clipper.addPath(polygon, pyclipr.Subject, False)  # False means closed path
    
    # Execute the union operation to merge all polygons
    result_paths = clipper.execute(pyclipr.Union, pyclipr.FillRule.NonZero)
    
    # Convert angle to radians
    angle_rad = math.radians(angle)
    
    # Calculate perpendicular angle for line direction
    perp_angle = angle_rad + math.pi/2
    
    # Calculate unit vector in perpendicular direction
    dir_x = math.cos(perp_angle)
    dir_y = math.sin(perp_angle)
    
    # Calculate the range needed to cover the bounding box
    min_x, min_y, max_x, max_y = bounds
    
    # Calculate the diagonal length of the bounding box (to ensure lines span the whole area)
    diagonal = math.sqrt((max_x - min_x)**2 + (max_y - min_y)**2)
    
    # Calculate the intersection point of the perpendicular line passing through the center of the bounding box
    center_x = (min_x + max_x) / 2
    center_y = (min_y + max_y) / 2
    
    # Calculate the perpendicular distance from center to the reference line
    proj_dist = center_x * math.cos(angle_rad) + center_y * math.sin(angle_rad)
    
    # Generate lines to cover the entire polygon
    spacing = line_spacing  # mm
    num_lines = int(diagonal / spacing) + 1
    
    # Create the line segments
    infill_lines = []
    
    for i in range(-num_lines, num_lines + 1):
        # Calculate the offset from the reference line
        dist = proj_dist + i * spacing
        
        # Calculate the normal vector for the line
        nx = math.cos(angle_rad)
        ny = math.sin(angle_rad)
        
        # Calculate the perpendicular direction
        px = -ny
        py = nx
        
        # Calculate a point on the line
        line_point_x = nx * dist
        line_point_y = ny * dist
        
        # Create a line long enough to span the entire bounding box
        line_len = diagonal * 2
        
        # Calculate the line endpoints
        start_point = (line_point_x - px * line_len, line_point_y - py * line_len)
        end_point = (line_point_x + px * line_len, line_point_y + py * line_len)
        
        # Create a line path for clipping
        line_path = np.array([start_point, end_point])
        
        # Create a new clipper to intersect the line with the polygons
        line_clipper = pyclipr.Clipper()
        line_clipper.scaleFactor = int(1000)
        
        # Add line as subject (open path)
        line_clipper.addPath(line_path, pyclipr.Subject, True)  # True for open path
        
        # Add unified polygons as clip
        for path in result_paths:
            line_clipper.addPath(path, pyclipr.Clip, False)  # False for closed paths
        
        # Execute the intersection 
        _, open_paths = line_clipper.execute(
            pyclipr.Intersection, 
            pyclipr.FillRule.NonZero,
            returnOpenPaths=True
        )
        
        # Convert the result to line segments
        for path in open_paths:
            if len(path) >= 2:
                # Extract start and end points
                infill_lines.append([(path[0][0], path[0][1]), (path[1][0], path[1][1])])
    
    return infill_lines


def slicingParametersPETG():
    return SlicingParameters(
        first_layer_height=0.3,
        layer_height=0.12,
        first_layer_extrusion_width=0.8,
        first_layer_temperature=250,
        num_perimeters=30,
        volume_rate_mm3_per_s=10.0,
        extrusion_width=0.6,
        nozzle_diameter=0.6,
        nozzle_temperature=220,
        travel_speed=250,
        perimeter_overlap=0.15,
        brim_width=3.0,
        bed_temperature=80,
        fan_speed=0,
        extrusion_multiplier=1.0,
    )


def slicingParametersPLA():
    return SlicingParameters(
        first_layer_height=0.3,
        layer_height=0.2,
        first_layer_extrusion_width=0.8,
        first_layer_temperature=220,
        num_perimeters=20,
        volume_rate_mm3_per_s=10.0,
        extrusion_width=0.6,
        nozzle_diameter=0.6,
        nozzle_temperature=200,
        travel_speed=250,
        perimeter_overlap=0.15,
        brim_width=0.0,
        bed_temperature=60,
        extrusion_multiplier=1.0,
    )


def slicingParametersABS():
    return SlicingParameters(
        first_layer_height=0.3,
        layer_height=0.2,
        first_layer_extrusion_width=0.8,
        first_layer_temperature=250,
        num_perimeters=20,
        volume_rate_mm3_per_s=10.0,
        extrusion_width=0.6,
        nozzle_diameter=0.6,
        nozzle_temperature=220,
        travel_speed=250,
        perimeter_overlap=0.15,
        brim_width=0.0,
        bed_temperature=80,
        extrusion_multiplier=1.0,
    )


def slicingParametersTPU():
    return SlicingParameters(
        first_layer_height=0.3,
        layer_height=0.2,
        first_layer_extrusion_width=0.8,
        first_layer_temperature=250,
        num_perimeters=2,
        volume_rate_mm3_per_s=5.0,
        extrusion_width=0.6,
        nozzle_diameter=0.6,
        nozzle_temperature=240,
        travel_speed=250,
        perimeter_overlap=0.15,
        brim_width=0.0,
        bed_temperature=0,
        extrusion_multiplier=1.2,
        infill_angle=45,
        infill_density=0.2,
        infill_layer_freq=1,
        infill_overlap=0.15
    )


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

    default_dir = os.path.dirname(input_file)
    default_filename = os.path.splitext(os.path.basename(input_file))[0] + ".gcode"
    output_file = save_file(default_dir, default_filename)
    if not output_file:
        print("No output file selected.")
        return

    slicing_params = slicingParametersTPU()

    list_of_pause_zheights = [6.1]

    pause_triggered = {pause: False for pause in list_of_pause_zheights}

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

    # Calculate bounding box bounds for infill generation
    bbox_min_x = bounding_box.GetMin().Coordinates[0]
    bbox_min_y = bounding_box.GetMin().Coordinates[1]
    bbox_max_x = bounding_box.GetMax().Coordinates[0]
    bbox_max_y = bounding_box.GetMax().Coordinates[1]
    bbox_bounds = [bbox_min_x, bbox_min_y, bbox_max_x, bbox_max_y]

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

        gcode_writer = GcodeWriter(f, slicing_params)
        gcode_writer.startPrint()
        gcode_writer.enableAbsolutePositioning()

        gcode_writer.setAcceleration()
        gcode_writer.setExtruderTemperatureWithoutWait(
            slicing_params.first_layer_temperature
        )
        gcode_writer.setBedTemperature()

        # Alternate infill angle every layer
        infill_angles = [slicing_params.infill_angle, slicing_params.infill_angle + 90]

        for i, z_height in enumerate(z_range):
            z_height_mm = slicing_params.first_layer_height + z_height / 1000.0
            z_height_in_model = z_height_mm + z_min
            layer_height = z_height_mm - previous_z
            previous_z = z_height_mm

            if i == 0:
                gcode_writer.setFanSpeed(0)
                gcode_writer.setSpeedFactor(50)

            if i == 2:
                gcode_writer.setFanSpeed(slicing_params.fan_speed)
                gcode_writer.setSpeedFactor(100)
                gcode_writer.setExtruderTemperatureWithoutWait(
                    slicing_params.nozzle_temperature
                )

            gcode_writer.layerChange(z_height_mm, layer_height)
            gcode_writer.startObject("build_plate")

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
                num_brim_lines = int(
                    math.ceil(
                        slicing_params.brim_width / slicing_params.extrusion_width
                    )
                )
                offsetRange = range(-num_brim_lines, slicing_params.num_perimeters)

            for j in offsetRange:
                contour = gladius.GenerateContour(z_height_in_model, offset)
                # Convert contour to pyclipr format and append to polygons
                polygons.extend(process_contour(contour))
                offset -= (
                    math.copysign(1, j)
                    * slicing_params.extrusion_width
                    * (1.0 - slicing_params.perimeter_overlap)
                )

            progress_dialog.Update(
                i + 1, f"Generating contours at Z={z_height_mm:.2f} mm"
            )

            sorted_perimeter = reduce_travel_moves(polygons)

            gcode_writer.addPolygons(sorted_perimeter, layer_height)
            
            # Add infill if it's a layer that should have infill
            if slicing_params.infill_density > 0 and i % slicing_params.infill_layer_freq == 0:
                # Get the innermost contour for infill
                innermost_offset = offset + slicing_params.extrusion_width * slicing_params.infill_overlap
                inner_contour = gladius.GenerateContour(z_height_in_model, innermost_offset)
                inner_polygons = process_contour(inner_contour)
                
                if len(inner_polygons) > 0:
                    # Calculate line spacing based on infill density
                    # Higher density = closer lines
                    line_spacing = slicing_params.extrusion_width / slicing_params.infill_density
                    
                    # Alternate the angle each layer
                    infill_angle = infill_angles[i % len(infill_angles)]
                    
                    # Generate infill pattern
                    infill_lines = generate_straight_line_infill(
                        inner_polygons, 
                        line_spacing, 
                        infill_angle, 
                        bbox_bounds
                    )
                    
                    # Convert infill lines to a format for gcode writer
                    infill_segments = []
                    for line in infill_lines:
                        infill_segments.append(line)
                    
                    # Write infill pattern to gcode
                    if infill_segments:
                        # First add a comment to indicate infill
                        gcode_writer.file.write(f";TYPE:Infill\n")
                        gcode_writer.file.write(f";WIDTH:{slicing_params.extrusion_width:.6f}\n")
                        
                        # Calculate infill feedrate - can be faster than perimeters
                        infill_track_area_mm2 = (
                            slicing_params.extrusion_width - layer_height
                        ) * layer_height + layer_height * layer_height * 0.25 * np.pi
                        
                        feedrate_mm_per_s = slicing_params.volume_rate_mm3_per_s / infill_track_area_mm2
                        feedrate_mm_per_min = feedrate_mm_per_s * 60.0
                        
                        nozzle_area = np.pi * (slicing_params.nozzle_diameter * 0.5) ** 2
                        extrusion_per_extruded_mm = infill_track_area_mm2 / nozzle_area * 0.1 * slicing_params.extrusion_multiplier
                        
                        # Set travel speed for initial move
                        travel_speed_mm_per_min = slicing_params.travel_speed * 60.0
                        
                        # Process each line segment
                        for segment in infill_segments:
                            start_point, end_point = segment
                            
                            # Move to start of segment
                            gcode_writer.file.write(
                                f"G1 X{start_point[0]:.4f} Y{start_point[1]:.4f} F{travel_speed_mm_per_min:.4f}\n"
                            )
                            
                            # Calculate distance for extrusion
                            distance = math.sqrt(
                                (end_point[0] - start_point[0]) ** 2 +
                                (end_point[1] - start_point[1]) ** 2
                            )
                            
                            extruder_distance = distance * extrusion_per_extruded_mm
                            gcode_writer.extruder_position += extruder_distance
                            
                            # Write feedrate for first segment only
                            gcode_writer.file.write(f"G1 F{feedrate_mm_per_min:.4f}\n")
                            
                            # Write the extrusion move
                            gcode_writer.file.write(
                                f"G1 X{end_point[0]:.4f} Y{end_point[1]:.4f} E{gcode_writer.extruder_position:.5f}\n"
                            )

            for pause in list_of_pause_zheights:
                if (not pause_triggered[pause]) and (z_height_mm > pause):
                    gcode_writer.addPause()
                    pause_triggered[pause] = True
                    # caveman debugging
                    print(f"Pause at Z={z_height_mm:.2f} mm")

            gcode_writer.endObject()

        gcode_writer.endPrint()
    progress_dialog.Destroy()


if __name__ == "__main__":
    try:
        main()
    except GladiusLib.EGladiusLibException as e:
        print(e)

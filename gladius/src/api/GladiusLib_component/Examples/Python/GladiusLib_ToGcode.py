import os
import sys
import wx

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", "..", "Bindings", "Python"))
import GladiusLib

def select_file():
    dialog = wx.FileDialog(None, "Select 3MF file", wildcard="*.3mf", style=wx.FD_OPEN | wx.FD_FILE_MUST_EXIST)
    if dialog.ShowModal() == wx.ID_CANCEL:
        return None
    return dialog.GetPath()

def select_gcode_start(default_dir):
    dialog = wx.FileDialog(None, "Select start gcode for your printer", wildcard="*.gcode", style=wx.FD_OPEN | wx.FD_FILE_MUST_EXIST, defaultDir=default_dir)
    if dialog.ShowModal() == wx.ID_CANCEL:
        return None
    return dialog.GetPath()

def save_file(default_dir, default_filename):
    dialog = wx.FileDialog(None, "Save G-code file", wildcard="*.gcode", style=wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT, defaultDir=default_dir, defaultFile=default_filename)
    if dialog.ShowModal() == wx.ID_CANCEL:
        return None
    return dialog.GetPath()

def main():
    app = wx.App(False)
    scriptpath = os.path.dirname(os.path.realpath(__file__))
    wrapper = GladiusLib.Wrapper(libraryName=os.path.join(scriptpath, "gladiuslib"))
    
    major, minor, micro = wrapper.GetVersion()
    print("GladiusLib version: {:d}.{:d}.{:d}".format(major, minor, micro), end="")
    print("")

    gladius = wrapper.CreateGladius()
    input_file = select_file()
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

    gladius.LoadAssembly(input_file)
    
    bounding_box = gladius.ComputeBoundingBox()
    print("Bounding box: [{:.2f}, {:.2f}, {:.2f}] x [{:.2f}, {:.2f}, {:.2f}]".format(
        bounding_box.GetMin().Coordinates[0], bounding_box.GetMin().Coordinates[1], bounding_box.GetMin().Coordinates[2],
        bounding_box.GetMax().Coordinates[0], bounding_box.GetMax().Coordinates[1], bounding_box.GetMax().Coordinates[2]
    ))

    # Generate contours at 0.2 mm intervals, starting from the bottom of the bounding box, moveing everything down to z=0
    z_min = bounding_box.GetMin().Coordinates[2]
    z_max = bounding_box.GetMax().Coordinates[2] - z_min
    z_step = 0.2

    z_range = range(0, int(z_max * 1000), int(z_step * 1000))
    total_steps = len(z_range)

    progress_dialog = wx.ProgressDialog("Generating Contours", "Progress", maximum=total_steps, parent=None, style=wx.PD_AUTO_HIDE | wx.PD_APP_MODAL)

    with open(output_file, 'w') as f:
        with open(gcode_start, 'r') as start_file:
            f.write(start_file.read())
            
        for i, z_height in enumerate(z_range):
            z_height_mm = z_height / 1000.0
            z_height_in_model = z_height_mm + z_min
            contour = gladius.GenerateContour(z_height_in_model, 0.0)
            print("Contours at Z={:.2f} mm: Number of polygons in contour: {}".format(z_height_in_model, contour.GetSize()))
            progress_dialog.Update(i + 1, f"Generating contours at Z={z_height_mm:.2f} mm")
            # Write the contour data to the G-code file
            f.write(f"; Contours at Z={z_height_mm:.2f} mm\n")
            contour.Begin()
            while (contour.Next()):
                if contour.GetSize() == 0:
                    continue
                polygon = contour.GetCurrentPolygon()
                while (polygon.Next()):
                    vertex = polygon.GetCurrentVertex()
                    f.write(f"G1 X{vertex.Coordinates[0]:.2f} Y{vertex.Coordinates[1]:.2f} Z{z_height_mm:.2f}\n")
                        
    progress_dialog.Destroy()

if __name__ == "__main__":
    try:
        main()
    except GladiusLib.EGladiusLibException as e:
        print(e)
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

def main():
    app = wx.App(False)
    libpath = os.path.dirname(os.path.realpath(__file__))
    wrapper = GladiusLib.Wrapper(libraryName=os.path.join(libpath, "gladiuslib"))
    
    major, minor, micro = wrapper.GetVersion()
    print("GladiusLib version: {:d}.{:d}.{:d}".format(major, minor, micro), end="")
    print("")

    gladius = wrapper.CreateGladius()
    input_file = select_file()
    if not input_file:
        print("No file selected.")
        return

    gladius.LoadAssembly(input_file)
    
    bounding_box = gladius.ComputeBoundingBox()
    print("Bounding box: [{:.2f}, {:.2f}, {:.2f}] x [{:.2f}, {:.2f}, {:.2f}]".format(
        bounding_box.GetMin().Coordinates[0], bounding_box.GetMin().Coordinates[1], bounding_box.GetMin().Coordinates[2],
        bounding_box.GetMax().Coordinates[0], bounding_box.GetMax().Coordinates[1], bounding_box.GetMax().Coordinates[2]
    ))

    
    z_min = bounding_box.GetMin().Coordinates[2]
    z_max = bounding_box.GetMax().Coordinates[2]
    z_step = 0.2

    z_range = range(int(z_min * 1000), int(z_max * 1000), int(z_step * 1000))
    total_steps = len(z_range)

    progress_dialog = wx.ProgressDialog("Generating Contours", "Progress", maximum=total_steps, parent=None, style=wx.PD_AUTO_HIDE | wx.PD_APP_MODAL)

    for i, z_height in enumerate(z_range):
        z_height_mm = z_height / 1000.0
        contour = gladius.GenerateContour(z_height_mm, 0.0)
        print("Contours at Z={:.2f} mm: Number of polygons in contour: {}".format(z_height_mm, contour.GetSize()))
        progress_dialog.Update(i + 1, f"Generating contours at Z={z_height_mm:.2f} mm")

    progress_dialog.Destroy()

if __name__ == "__main__":
    try:
        main()
    except GladiusLib.EGladiusLibException as e:
        print(e)
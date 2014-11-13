#!/usr/bin/python
# -*- coding: iso-8859-1 -*-

"""Various helper functions related to VTK.
"""

import vtk

def set_mapper_input(mapper, data):
    """ VTK 5 and 6 function to set a mapper input """
    try:
        mapper.SetInput(data)
    except AttributeError:
        mapper.SetInputData(data)

def pointCloudActor(points, color = (0, 0, 0, 255)):
    """Create a vtkActor representing a point cloud.

    Args:
      points: iterable of points [[x0, y0, z0], [x1, y1, z1], ...]
      color: color of the points in the RBBA format

    Returns:
      An instance of vtkActor
    """
    vtk_points = vtk.vtkPoints()
    vertices = vtk.vtkCellArray()
    colors = vtk.vtkUnsignedCharArray()
    colors.SetNumberOfComponents(4)
    colors.SetName("Colors")
    for (x, y, z) in points:
        point_id = vtk_points.InsertNextPoint(x, y, z)
        vertices.InsertNextCell(1)
        vertices.InsertCellPoint(point_id)
        colors.InsertNextTuple4(color[0], color[1], color[2], color[3])
    poly_data = vtk.vtkPolyData()
    poly_data.SetPoints(vtk_points)
    poly_data.SetVerts(vertices)
    poly_data.GetPointData().SetScalars(colors)
    mapper = vtk.vtkPolyDataMapper()
    set_mapper_input(mapper, poly_data)
    actor = vtk.vtkActor()
    actor.SetMapper(mapper)
    return actor


def boxesActor(boxes, color = (0, 0, 0, 100), wireframe=False):
    """Create a vtkActor representing a list of hexahedron.

    The hexahedrons are assumed to be aligned with the coordinate axis, and are
    given using their extremal points.


    Args:
      box: [[[xmin, ymin, zmin], [xmax, ymax, zmax]], ...]
      color: RGBA color
      wireframe: if True, the boxes are drawn in wireframe mode
    """
    grid = vtk.vtkUnstructuredGrid()
    points = vtk.vtkPoints()
    colors = vtk.vtkUnsignedCharArray()
    colors.SetNumberOfComponents(4)
    colors.SetName("Colors")
    for (index, box) in enumerate(boxes):
        colors.InsertNextTuple4(color[0], color[1], color[2], 100)
        P0 = [box[0][0], box[0][1], box[0][2]]
        P1 = [box[1][0], box[0][1], box[0][2]]
        P2 = [box[1][0], box[1][1], box[0][2]]
        P3 = [box[0][0], box[1][1], box[0][2]]
        P4 = [box[0][0], box[0][1], box[1][2]]
        P5 = [box[1][0], box[0][1], box[1][2]]
        P6 = [box[1][0], box[1][1], box[1][2]]
        P7 = [box[0][0], box[1][1], box[1][2]]
        points.InsertNextPoint(P0)
        points.InsertNextPoint(P1)
        points.InsertNextPoint(P2)
        points.InsertNextPoint(P3)
        points.InsertNextPoint(P4)
        points.InsertNextPoint(P5)
        points.InsertNextPoint(P6)
        points.InsertNextPoint(P7)
        hexa = vtk.vtkHexahedron()
        hexa.GetPointIds().SetNumberOfIds(8)
        for i in range(8):
            hexa.GetPointIds().SetId(i, 8 * index + i)
        grid.InsertNextCell(hexa.GetCellType(), hexa.GetPointIds())
    grid.SetPoints(points)
    grid.GetCellData().SetScalars(colors)
    mapper = vtk.vtkDataSetMapper()
    set_mapper_input(mapper, grid)
    actor = vtk.vtkActor()
    actor.SetMapper(mapper)
    if wireframe:
        actor.GetProperty().SetRepresentationToWireframe()
        actor.GetProperty().SetLineWidth(2.)
    return actor


def quadsActor(bounds, color):
    """Create solid, axis-aligned quads at 0 in Z.

    Args:
      bounds: [[[xmin, ymin], [xmax, ymax]], ...]
      color: [R, G, B, A]
    """
    points = vtk.vtkPoints()
    quads = vtk.vtkCellArray()
    colors = vtk.vtkUnsignedCharArray()
    colors.SetNumberOfComponents(4)
    for (index, bound) in enumerate(bounds):
        colors.InsertNextTuple4(*color)
        (low, high) = bound
        points.InsertNextPoint(low[0], low[1], 0)
        points.InsertNextPoint(high[0], low[1], 0)
        points.InsertNextPoint(high[0], high[1], 0)
        points.InsertNextPoint(low[0], high[1], 0)
        quad = vtk.vtkQuad()
        for i in range(4):
            quad.GetPointIds().SetId(i, 4 * index + i)
        quads.InsertNextCell(quad)
    poly_data = vtk.vtkPolyData()
    poly_data.SetPoints(points)
    poly_data.SetPolys(quads)
    poly_data.GetCellData().SetScalars(colors)
    mapper = vtk.vtkPolyDataMapper()
    set_mapper_input(mapper, poly_data)
    actor = vtk.vtkActor()
    actor.SetMapper(mapper)
    actor.RotateZ(180)
    actor.RotateY(180)
    return actor


def legendActor(text):
    """Display the given text in the lower left corner of the window.
    """
    actor = vtk.vtkTextActor()
    actor.GetTextProperty().SetFontSize(24)
    actor.SetInput(text)
    actor.GetTextProperty().SetColor(0, 0, 0)
    return actor


def getRenderWindowInteractor(renderer, interactor_style):
    """Return a vtkRenderWindowInteractor from a renderer and interactor style.
    """
    render_window = vtk.vtkRenderWindow()
    render_window.AddRenderer(renderer)
    render_window_interactor = vtk.vtkRenderWindowInteractor()
    render_window_interactor.SetInteractorStyle(interactor_style)
    render_window_interactor.SetRenderWindow(render_window)
    render_window_interactor.Initialize()
    render_window.Render()
    return render_window_interactor

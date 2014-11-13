#!/usr/bin/python
# -*- coding: iso-8859-1 -*-

"""Use VTK to display an HMatrix graphically.
"""

import logging
import optparse
import sys
import vtk
import random

from hmat_json_parser import *
from vtk_utils import *


def badLeaves(hmat):
    """Return a list of big, badly-compressed Rk leaves.
    """
    # leaves = rkLeavesByCompressionRatio(hmat)
    leaves = rkLeavesByK(hmat)
    return [x[1] for x in leaves if x[1].rows['n'] > 3000]


def clusterTreeActor(cluster_tree, level=-1):
    nodes = cluster_tree.nodes(level, [])
    boxes = [node.bounding_box for node in nodes]
    return boxesActor(boxes, [255, 0, 0, 100], True)


class MyInteractorStyle(vtk.vtkInteractorStyleTrackballCamera):
    """Custom interactor to change display on key press.
    """
    def __init__(self, hmat, hmat_render_window, parent=None):
        self.hmat = hmat
        self.hmat_render_window = hmat_render_window
        self.block_actor = None
        self.legend_actor = None
        self.rk_leaves = hmat.listRkLeaves()
        random.shuffle(self.rk_leaves)
        self.rk_leaves = badLeaves(hmat)
        self.rk_leaves.reverse()
        self.full_leaves = hmat.listFullLeaves()
        random.shuffle(self.full_leaves)
        self.AddObserver('KeyPressEvent', self.OnKeyPress)
        self.row_actor = None
        self.col_actor = None
        self.cluster_tree = ClusterTree.fromHMatrix(hmat)
        self.tree_actor = None
        self.level = -1
        self.index = 1

    def OnKeyPress(self, obj, event):
        key = self.GetInteractor().GetKeySym()
        renderer = self.GetInteractor().GetRenderWindow().GetRenderers().GetFirstRenderer()
        if key == 's':
            print "Capture"
            windowToImageFilter = vtk.vtkWindowToImageFilter()
            windowToImageFilter.SetInput(self.GetInteractor().GetRenderWindow())
            windowToImageFilter.SetMagnification(1.)
            windowToImageFilter.SetInputBufferTypeToRGBA()
            windowToImageFilter.Update()
            writer = vtk.vtkPNGWriter()
            writer.SetFileName("screenshot_%02d.png" % self.index)
            writer.SetInputConnection(windowToImageFilter.GetOutputPort())
            writer.Write()
            self.index += 1
        elif key == 'k':
            if self.rk_leaves == []:
                print "plus de feuilles !"
                return
            leaf = self.rk_leaves.pop()
            print leaf.describeLeaf()
            if self.row_actor is not None:
                renderer.RemoveActor(self.row_actor)
                renderer.RemoveActor(self.col_actor)
            self.row_actor = boxesActor([leaf.rows['boundingBox']], (0, 255, 0))
            self.col_actor = boxesActor([leaf.cols['boundingBox']], (0, 255, 0))
            renderer.AddActor(self.row_actor)
            renderer.AddActor(self.col_actor)
            self.GetInteractor().Render()
            if self.hmat_render_window is None:
                return
            # Affiche le bloc dans la HMatrix de l'autre cote
            hmat_renderer = self.hmat_render_window.GetRenderers().GetFirstRenderer()
            if self.block_actor is not None:
                hmat_renderer.RemoveActor(self.legend_actor)
                hmat_renderer.RemoveActor(self.block_actor)
            self.block_actor = quadsActor(
                [((leaf.cols['offset'], leaf.rows['offset']),
                  (leaf.cols['offset'] + leaf.cols['n'], leaf.rows['offset'] + leaf.rows['n']))],
                [0, 255, 0, 255])
            self.legend_actor = legendActor(leaf.describeLeaf(True))
            hmat_renderer.AddActor(self.block_actor)
            hmat_renderer.AddActor(self.legend_actor)
            self.hmat_render_window.Render()

        elif key == 'f':
            if self.full_leaves == []:
                print "plus de feuilles !"
                return
            leaf = self.full_leaves.pop()
            print leaf.describeLeaf()
            if self.row_actor is not None:
                renderer.RemoveActor(self.row_actor)
                renderer.RemoveActor(self.col_actor)
            self.row_actor = boxesActor([leaf.rows['boundingBox']], (255, 0, 0))
            self.col_actor = boxesActor([leaf.cols['boundingBox']], (255, 0, 0))
            renderer.AddActor(self.row_actor)
            renderer.AddActor(self.col_actor)
            self.GetInteractor().Render()
            # Affiche le bloc dans la HMatrix de l'autre cote
            if self.hmat_render_window is None:
                return
            hmat_renderer = self.hmat_render_window.GetRenderers().GetFirstRenderer()
            if self.block_actor is not None:
                hmat_renderer.RemoveActor(self.legend_actor)
                hmat_renderer.RemoveActor(self.block_actor)
            self.block_actor = quadsActor(
                [((leaf.cols['offset'], leaf.rows['offset']),
                  (leaf.cols['offset'] + leaf.cols['n'], leaf.rows['offset'] + leaf.rows['n']))],
                [255, 0, 0, 255])
            self.legend_actor = legendActor(leaf.describeLeaf(True))
            hmat_renderer.AddActor(self.block_actor)
            hmat_renderer.AddActor(self.legend_actor)
            self.hmat_render_window.Render()

        elif key == 't':
            if self.tree_actor is not None:
                renderer.RemoveActor(self.tree_actor)
            self.tree_actor = clusterTreeActor(self.cluster_tree, self.level)
            if self.tree_actor.GetMapper().GetInput().GetNumberOfPoints() == 0:
                self.level = -1
            else:
                self.level += 1
            renderer.AddActor(self.tree_actor)
            self.GetInteractor().Render()


def drawLines(lines):
    points = vtk.vtkPoints()
    vtk_lines = vtk.vtkCellArray()
    colors = vtk.vtkUnsignedCharArray()
    colors.SetNumberOfComponents(3)
    index = 0
    for line in lines:
        (start, end) = line
        points.InsertNextPoint(start[0], start[1], start[2])
        points.InsertNextPoint(end[0], end[1], end[2])
        vtk_line = vtk.vtkLine()
        vtk_line.GetPointIds().SetId(0, index)
        vtk_line.GetPointIds().SetId(1, index + 1)
        index += 2
        vtk_lines.InsertNextCell(vtk_line)
        colors.InsertNextTuple3(0, 0, 0)
    poly_data = vtk.vtkPolyData()
    poly_data.SetPoints(points)
    poly_data.SetLines(vtk_lines)
    poly_data.GetCellData().SetScalars(colors)
    return poly_data


def collectLines(hmat, lines):
    if not hmat.is_leaf:
        for child in hmat.children:
            collectLines(child, lines)
        low_x = hmat.cols['offset']
        mid_x = hmat.children[0].cols['offset'] + hmat.children[0].cols['n']
        high_x = hmat.cols['offset'] + hmat.cols['n']
        low_y = hmat.rows['offset']
        mid_y = hmat.children[0].rows['offset'] + hmat.children[0].rows['n']
        high_y = hmat.rows['offset'] + hmat.rows['n']
        lines.append([(low_x, mid_y, 0), (high_x, mid_y, 0)])
        lines.append([(mid_x, low_y, 0), (mid_x, high_y, 0)])


def hmatActor(hmat):
    m = hmat.rows['n']
    n = hmat.cols['n']
    # poly_data = drawRectangle(m, n)
    lines = []
    lines += [[(0, 0, 0), (n, 0, 0)],
              [(n, 0, 0), (n, m, 0)],
              [(n, m, 0), (0, m, 0)],
              [(0, m, 0), (0, 0, 0)]]
    collectLines(hmat, lines)
    poly_data = drawLines(lines)
    mapper = vtk.vtkPolyDataMapper()
    set_mapper_input(mapper, poly_data)
    actor = vtk.vtkActor()
    actor.SetMapper(mapper)
    actor.RotateZ(180)
    actor.RotateY(180)
    return actor


def fullLeavesActor(hmat):
    full_leaves = hmat.listFullLeaves()
    quads = []
    for leaf in full_leaves:
        quads.append(((leaf.cols['offset'], leaf.rows['offset']),
                      (leaf.cols['offset'] + leaf.cols['n'], leaf.rows['offset'] + leaf.rows['n'])))
    actor = quadsActor(quads, [255, 0, 0, 255])
    return actor


if __name__ == "__main__":
    logging.basicConfig(
        level=logging.INFO, format="%(levelname)s:%(asctime)s: %(message)s")
    parser = optparse.OptionParser()
    parser.add_option("--hmat", help="Display the HMatrix", action="store_true", dest="display_hmat")
    parser.add_option("--tree", help="Display the tree", action="store_true", dest="display_tree")
    (options, args) = parser.parse_args()

    logging.info('Lecture et Parsing du fichier')
    hmat = HMatrixStructure.fromFile(args[0])
    rows = ClusterTree.fromHMatrix(hmat)

    renderer = None
    interactor = None
    if options.display_hmat:
        renderer = vtk.vtkRenderer()
        renderer.SetBackground(1, 1, 1)
        renderer.AddActor(fullLeavesActor(hmat))
        renderer.AddActor(hmatActor(hmat))
        renderer.GetActiveCamera().ParallelProjectionOn()
        renderer.ResetCamera()
        interactor = getRenderWindowInteractor(renderer,
                                               vtk.vtkInteractorStyleImage())

    if options.display_tree:
        renderer2 = vtk.vtkRenderer()
        point_cloud_actor = pointCloudActor(hmat.points, (0, 0, 0, 70))
        renderer2.AddActor(point_cloud_actor)
        renderer2.SetBackground(1, 1, 1)
        render_window = interactor.GetRenderWindow() if renderer else None
        interactor_style = MyInteractorStyle(hmat, render_window)
        interactor2 = getRenderWindowInteractor(renderer2, interactor_style)
        interactor2.GetRenderWindow().SetPosition(300, 300)
        interactor2.Start()
    elif options.display_hmat:
        interactor.Start()


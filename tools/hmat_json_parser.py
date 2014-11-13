#!/usr/bin/python
# -*- coding: iso-8859-1 -*-

"""Parse a JSON file containing a dump of an HMatrix structure.
"""

import json
import pickle
import operator
import numpy as np
import math


class HMatrixStructure(object):
    """Represent the HMatrix tree.
    """
    def __init__(self, tree_dict, points, indices):
        """Create a node based on a dictionary.

        Args:
          tree_dict: the dictionary.
          points: location of the points
          indices: mapping
        """
        self.points = points
        self.indices = indices
        self.is_leaf = tree_dict['isLeaf']
        self.depth = tree_dict['depth']
        self.rows = tree_dict['rows']
        self.cols = tree_dict['cols']
        if self.is_leaf:
            self.children = None
            self.leaf_type = tree_dict['leaf_type']
            if self.leaf_type == 'Rk':
                self.k = tree_dict['k']
                self.eta = tree_dict['eta']
        else:
            self.children = []
            for child in tree_dict['children']:
                self.children.append(HMatrixStructure(child, points, indices))

    @classmethod
    def fromFile(cls, filename):
        tree = json.load(file(filename))
        hmat = HMatrixStructure(tree['tree'], tree['points'], tree['mapping'])
        return hmat

    def compressionRatio(self):
        """Return Memory_compressed / Memory_full for Rk leaves.
        """
        return (float(self.rows['n'] + self.cols['n']) * self.k
                / float(self.rows['n'] * self.cols['n']))

    def listNodes(self, nodes=None):
        """Return a list of nodes.

        Args:
          nodes: if not None, list to append to.

        Returns:
          a list of nodes (instances of HMatrixStructure)
        """
        if nodes is None:
            nodes = []
        nodes.append(self)
        if not self.is_leaf:
            for child in self.children:
                child.listNodes(nodes)
        return nodes

    def listLeaves(self):
        """Return the list of leaves.
        """
        return [x for x in self.listNodes() if x.is_leaf]

    def listRkLeaves(self):
        """Return the list of Rk leaves.
        """
        return [x for x in self.listNodes() if x.is_leaf and x.leaf_type == 'Rk']

    def listFullLeaves(self):
        """Return the list of Full leaves.
        """
        return [x for x in self.listNodes() if x.is_leaf and x.leaf_type != 'Rk']

    def admissibilityParameters(self):
        """Return the tuple (rows_diameter, cols_diameter, distance).

        These are the parameters required to evaluate the critical eta.
        """
        def diameter(bounding_box):
            distance = np.linalg.norm(np.array(bounding_box[0]) - np.array(bounding_box[1]))
            return distance
        def distanceTo(bounding_box1, bounding_box2):
            result = 0.
            difference = 0.
            for i in range(3):
                difference = max(0., bounding_box1[0][i] - bounding_box2[1][i])
                result += difference * difference
                difference = max(0., bounding_box2[0][i] - bounding_box1[1][i])
                result += difference * difference
            return math.sqrt(result)
        assert self.leaf_type == 'Rk'
        row_diameter = diameter(self.rows['boundingBox'])
        col_diameter = diameter(self.cols['boundingBox'])
        distance = distanceTo(self.rows['boundingBox'], self.cols['boundingBox'])
        return (row_diameter, col_diameter, distance)

    def describeLeaf(self, short=False):
        """Print a description of this leaf to stdout.
        """
        if self.leaf_type == 'Rk':
            (rows_diameter, cols_diameter, distance) = self.admissibilityParameters()
            eta = min(rows_diameter, cols_diameter) / distance
            template = """Feuille Rk (%d, %d)
  k = %d
  Taux de compression = %f%%""" % (self.rows['n'], self.cols['n'],
                                   self.k, 100 * self.compressionRatio())
            if not short:
                template +="""
  Diametre rows = %f
  Diametre cols = %f
  Distance      = %f
  eta           = %f""" % (rows_diameter, cols_diameter, distance, eta)
            return template
        else:
            template = "Feuille Full (%d, %d)" % (self.rows['n'], self.cols['n'])
            return template



class ClusterTree(object):
    """Get the rows cluster tree back from the HMatrixStructure.
    """
    def __init__(self, bounding_box, points, offset, n, depth):
        """Do not call.
        """
        self.bounding_box = bounding_box
        self.points = points
        self.offset = offset
        self.n = n
        self.children = []
        self.depth = depth

    def nodes(self, level=-1, result=[]):
        """Return the list of nodes of the ClusterTree.
        """
        if self.depth == level or level == -1:
            result.append(self)
        if self.depth < level or level == -1:
            for child in self.children:
                child.nodes(level, result)
        return result

    @classmethod
    def fromHMatrix(cls, hmat, depth=0):
        """
        """
        result = ClusterTree(hmat.rows['boundingBox'], hmat.points,
                             hmat.rows['offset'], hmat.rows['n'], depth)
        if not hmat.is_leaf:
            result.children.append(ClusterTree.fromHMatrix(hmat.children[0], depth + 1))
            result.children.append(ClusterTree.fromHMatrix(hmat.children[-1], depth + 1))
        return result


def computeSparsity(hmat):
    nodes = hmat.listNodes()
    nodes_by_rows = {}
    nodes_by_cols = {}
    for node in nodes:
        rows = (node.rows['offset'], node.rows['n'])
        cols = (node.cols['offset'], node.cols['n'])
        nodes_by_rows[rows] = nodes_by_rows.get(rows, 0) + 1
        nodes_by_cols[cols] = nodes_by_cols.get(cols, 0) + 1
    return max(max(nodes_by_cols.values(), nodes_by_rows.values()))


def rkLeavesByCompressionRatio(hmat):
    """Sort the Rk leaves by descending compression ratio.

    Args:
      hmat: an instance of HMatrixStructure

    Returns:
      [(compression_ratio1, leaf1), ...]
    """
    leaves = hmat.listRkLeaves()
    ratio_leaves = [(x.compressionRatio(), x) for x in leaves]
    ratio_leaves.sort(key=operator.itemgetter(0), reverse=True)
    return ratio_leaves


def rkLeavesByK(hmat):
    """Sort the Rk leaves by descending compression ratio.

    Args:
      hmat: an instance of HMatrixStructure

    Returns:
      [(compression_ratio1, leaf1), ...]
    """
    leaves = hmat.listRkLeaves()
    ratio_leaves = [(x.k, x) for x in leaves]
    ratio_leaves.sort(key=operator.itemgetter(0), reverse=True)
    return ratio_leaves

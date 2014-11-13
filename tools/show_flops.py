#!/usr/bin/python
import sys
import parse_json_perf_reports as reports


def getLeaves(tree):
    result = []
    if len(tree.children) == 0:
        return [tree]
    else:
        for child in tree.children:
            result += getLeaves(child)
    return result


def aggregate(leaves):
    result = {}
    for leaf in leaves:
        grouped_leaves = result.get(leaf.label, [])
        grouped_leaves.append(leaf)
        result[leaf.label] = grouped_leaves
    return result


def computeAggregates(grouped_leaves):
    nodes = []
    for (label, leaves) in grouped_leaves.items():
        data = reports.TraceData()
        data.n = sum([leaf.data.n for leaf in leaves])
        data.time_in_s = sum([leaf.data.time_in_s for leaf in leaves])
        data.flops = sum([leaf.data.flops for leaf in leaves])
        node = reports.TraceNode(label, data)
        nodes.append(node)
    return nodes

if __name__ == "__main__":
    filename = sys.argv[1]
    all_reports = reports.parseReportsInJsonFile(filename)
    leaves = []
    for report in all_reports.values():
        leaves += getLeaves(report)
    grouped_leaves = aggregate(leaves)
    aggregates = computeAggregates(grouped_leaves)
    for node in aggregates:
        node.prettyPrint()

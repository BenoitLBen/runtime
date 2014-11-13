#!/usr/bin/python
import logging
import sys
import json
import parse_json_perf_reports as perf_reports

"""
"""

# Set logging message formatting
logging.basicConfig()
logger = logging.getLogger()
logger.setLevel(logging.INFO)


def parseReports(filename):
    f = open(filename, "r")
    file_content = f.read()
    json_content = json.loads(file_content)
    reports = []
    for tree in json_content:
        reports.append(perf_reports.TraceNode.fromJson(tree))
    return reports


def listNodes(tree):
    result = [tree]
    for child in tree.children:
        result += listNodes(child)
    return result

def mergeTrees(t1, t2):
    assert t1.label == t2.label
    t1.data.time_in_s += t2.data.time_in_s
    t1.data.n += t2.data.n
    t1.data.flops += t2.data.flops
    t1.data.bytes_sent += t2.data.bytes_sent
    t1.data.bytes_received += t2.data.bytes_received
    t1.data.comm_time += t2.data.comm_time
    children_by_label = {}
    for child in t1.children:
        children_by_label[child.label] = [child]
    for child in t2.children:
        if child in children_by_label:
            children_by_label[child.label].append(child)
        else:
            children_by_label[child.label] = [child]
    t1.children = []
    for label in children_by_label:
        children = children_by_label[label]
        if len(children_by_label[label]) == 2:
            mergeTrees(children[0], children[1])
        t1.children.append(children[0])


def recomputeTimes(node):
    if node.children == []:
        return node
    else:
        total_time = 0
        for child in node.children:
            child = recomputeTimes(child)
            total_time += child.data.time_in_s
        if total_time > node.data.time_in_s:
            node.data.time_in_s += total_time
        return node


def aggregateReports(reports):
    master_report = reports[0]
    id_to_node = {node.identifier: node for node in listNodes(reports[0])}
    for report in reports[1:]:
        identifier = report.label.split("-")[1][1:]
        node = id_to_node[identifier]
        for child in report.children:
            node.data.time_in_s += child.data.time_in_s
        for child in report.children:
            for c in node.children:
                if c.label == child.label:
                    mergeTrees(c, child)
                    break
            else:
                node.children.append(child)
    return recomputeTimes(master_report)


if __name__ == "__main__":
    reports = parseReports(sys.argv[1])
    aggregate = aggregateReports(reports)
    out_file = open(sys.argv[2], "w")
    out_file.write("[")
    aggregate.toJson(out_file)
    out_file.write("]")


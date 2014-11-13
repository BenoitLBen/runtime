#!/usr/bin/python
import logging
import sys
import json


"""Analyze tracing reports from HMat.
"""

# Set logging message formatting
logging.basicConfig()
logger = logging.getLogger()
logger.setLevel(logging.INFO)


class TraceData(object):
  """Represent data associated with a tracing node, as in trace_calls.c.
  """
  def __init__(self,
               time_in_s='0.', n='0', flops='0',
               bytes_sent='0', bytes_received='0', comm_time='0.0'):
    """
    """
    self.time_in_s =float(time_in_s[:-1])
    self.n = int(n)
    self.flops = int(flops)
    self.bytes_sent = int(bytes_sent)
    self.bytes_received = int(bytes_received)
    self.comm_time = float(comm_time[:-1])

  @classmethod
  def fromJson(cls, json_dict):
    """Create an instance of TraceData from a JSON dictionary.

    Args:
      json_dict: the JSON dictionary
    Returns:
      an instance of TraceData
    """
    data = TraceData()
    data.time_in_s = float(json_dict["totalTime"])
    data.n = int(json_dict["n"])
    data.flops = float(json_dict["totalFlops"])
    data.bytes_sent = float(json_dict["totalBytesSent"])
    data.bytes_received = float(json_dict["totalBytesReceived"])
    data.comm_time = float(json_dict["totalCommTime"])
    return data

  def prettyPrintBytes(cls, value):
    suffix = ''
    value = float(value)
    if value > 1024:
      value = float(value) / 1024
      suffix = 'k'
    if value > 1024:
      value = float(value) / 1024
      suffix = 'M'
    if value > 1024:
      value = float(value) / 1024
      suffix = 'G'
    return "%.3f%s" % (value, suffix)
  prettyPrintBytes = classmethod(prettyPrintBytes)

  def prettyPrintFlops(cls, value):
    suffix = ''
    value = float(value)
    if value > 1000:
      value = float(value) / 1000
      suffix = 'k'
    if value > 1024:
      value = float(value) / 1000
      suffix = 'M'
    if value > 1024:
      value = float(value) / 1000
      suffix = 'G'
    return "%.3f%s" % (value, suffix)
  prettyPrintFlops = classmethod(prettyPrintFlops)


class TraceNode(object):
  """Represent a tracing tree following the same structure as in trace_calls.c.
  """
  def __init__(self, label, data, parent=None):
    self.parent = parent
    self.children = []
    self.label = label
    self.data = data
    self.children_total = None

  @classmethod
  def fromJson(cls, json_dict, parent=None):
    """Create an instance of TraceNode from a JSON dictionay.

    Args:
      json_dict: the JSON dict.
      parent: Parent of the current node.

    Returns:
      A tree of TraceNode.
    """
    node = TraceNode(json_dict["name"], TraceData.fromJson(json_dict), parent)
    for child in json_dict["children"]:
      node.children.append(TraceNode.fromJson(child, node))
    node.identifier = json_dict["id"]
    return node

  def toJson(self, f):
    """Dump the current tree to a file in JSON format.

    Args:
      f: a file-like object
    """
    result = '{"name": "%s", "identifier": "%s", "n": %d, "totalTime": %f,\
"totalFlops": %d, "totalBytesSent": %d, "totalBytesReceived": %d, \
"totalCommTime": %f, "children": [' % (self.label, self.identifier, self.data.n,
                                       self.data.time_in_s, self.data.flops,
                                       self.data.bytes_sent, self.data.bytes_received,
                                       self.data.comm_time)
    f.write(result)
    delimiter = ""
    for child in self.children:
      f.write(delimiter)
      child.toJson(f)
      delimiter = ", "
    f.write("]}")

  def computeChildrenSum(self):
    self.children_total = self.computeSum()
    if self.label == "root":
      self.data.time_in_s = sum([x.data.time_in_s for x in self.children])
      self.data.comm_time = self.children_total.comm_time

  def computeSum(self):
    """Computes the aggregates over all the children of a given node.

      Returns:
        A TraceData structure containing the aggregates.
    """
    if self.children_total is not None:
      return self.children_total
    data_sum = TraceData()
    data_sum.flops = self.data.flops
    data_sum.bytes_sent = self.data.bytes_sent
    data_sum.bytes_received = self.data.bytes_received
    data_sum.comm_time = self.data.comm_time
    if self.children is not None:
      for child in self.children:
        child_sum = child.computeSum()
        data_sum.time_in_s += child.data.time_in_s
        data_sum.flops += child_sum.flops
        data_sum.bytes_sent += child_sum.bytes_sent
        data_sum.bytes_received += child_sum.bytes_received
        data_sum.comm_time += child_sum.comm_time
    self.children_total = data_sum
    return data_sum

  def prettyPrint(self, depth=0):
    prefix = '    ' * depth
    data_sum = self.computeSum()
    if self.label == "root":
      self.data.time_in_s = sum([x.data.time_in_s for x in self.children])
      self.data.comm_time = data_sum.comm_time
    children_percentage = 0.
    comm_percentage = 0.
    children_flops_per_s = 0.
    try:
      children_flops_per_s = data_sum.flops / self.data.time_in_s
    except ZeroDivisionError, e:
      pass
    try:
      children_percentage = (
        100 * float(data_sum.time_in_s) / self.data.time_in_s)
      comm_percentage = 100 * float(data_sum.comm_time) / self.data.time_in_s
    except ZeroDivisionError, e:
      pass
    print "%s%s:" % (prefix, self.label)
    print "  %sNumber of times called: %d" % (prefix, self.data.n)
    print "  %sTotal Time Spent in Function: %.3fs" % (prefix,
                                                      self.data.time_in_s)
    print "  %sTotal Time Spent in Children: %.3fs (%.3f%%)" % (
      prefix, data_sum.time_in_s, children_percentage)
    print  "  %sTotal Comm Time (including children): %.3fs (%.3f%%)" % (
      prefix, data_sum.comm_time, comm_percentage )
    print "  %sTotal Bytes Received (including children): %s" % (prefix,
      TraceData.prettyPrintBytes(data_sum.bytes_received))
    print "  %sTotal Bytes Sent (including children): %s" % (prefix,
      TraceData.prettyPrintBytes(data_sum.bytes_sent))
    print "  %sTotal Flops (Flops/s): %sFlops (%sFlops/s)" % (
      prefix, TraceData.prettyPrintFlops(data_sum.flops),
      TraceData.prettyPrintFlops(children_flops_per_s))
    for child in self.children:
      child.prettyPrint(depth + 1)

  def printTree(self, depth=0):
    print "%s%s" % ('  ' * depth, self.label)
    for child in self.children:
      child.printTree(depth + 1)


class MalFormedFileException(Exception):
  """Exception raised in case of a malformed file.
  """
  def __init__(self, value):
    self.value = value

  def __str__(self):
    return repr(self.value)


def findReports(file_content):
  """Extracts the reports from a file.

    A report is delimited by the marker "---Tracing Report" at the beginning
    and the marker "---End of Tracing Report" at the end. This function returns
    a dictionnary of reports, each report being a list of its lines.
    The key in this dictionnary is the node number this report is referring to.

  Args:
    file_content: a string containing the file content

  Return:
    A dictionnary of reports

  Raise:
    MalFormedFileException if the file is not properly formatted.
  """
  REPORT_START_MARKER = "---Tracing Report #"
  REPORT_END_MARKER = "---End of Tracing Report #"
  result = {}
  file_content_lines = file_content.split('\n')
  line_index = 0
  while line_index < len(file_content_lines):
    if REPORT_START_MARKER in file_content_lines[line_index]:
      process_id = int(
        file_content_lines[line_index][len(REPORT_START_MARKER):])
      logger.info("Found report #%d" % process_id)
      if process_id in result:
        error_str = "Bogus file. Duplicated report at line %d" % line_index
        logger.error(error_str)
        raise MalFormedFileException(error_str)
      report = []
      line_index += 1
      while line_index < len(file_content_lines):
        if REPORT_END_MARKER in file_content_lines[line_index]:
          end_process_id = int(
            file_content_lines[line_index][len(REPORT_END_MARKER):])
          if end_process_id != process_id:
            error_message = ("Malformed file. End of report doesn't match the "
                             "start of the report (%d != %d). Line %d" %
                             (end_process_id, process_id, line_index))
            logger.error(error_message)
            raise MalFormedFileException(error_message)
          line_index += 1
          break
        report.append(file_content_lines[line_index])
        line_index += 1
      else:
        logger.warning("End of file reached before report end.File truncated?")
      result[process_id] = report
    line_index += 1
  return result


def parseReport(report):
  """Converts a reports in a TraceNode instance.

  Args:
    report: the report, given as a list of strings

  Returns:
    an instance of TraceNode
  """
  REPORT_HEADER = ("function_name,total_time_in_s,n,total_flops,"
                   "total_bytes_sent,total_bytes_received,total_comm_time_in_s")
  current_node = None
  current_depth = 0
  assert report[0].strip() == REPORT_HEADER
  for line in report[1:]:
    if len(line.strip()) == 0:
      continue
    first_non_space_index = min([i for i in range(len(line))
                                 if line[i] != ' '])
    line = line.strip()
    line_tokens = line.split('|')
    name = line_tokens[0]
    assert len(line_tokens) == 7
    depth = first_non_space_index / 2
    assert depth <= current_depth + 1
    assert (depth > 0) or (name == "root")
    data = TraceData(line_tokens[1], line_tokens[2], line_tokens[3],
                     line_tokens[4], line_tokens[5], line_tokens[6])
    if name == "root":
      assert current_node is None
      current_node = TraceNode("root", data, None)
    else:
      if current_depth > depth:
        # depth is smaller than the current one, so we go up.
        # When we're done, we are in the case current_depth = depth
        while current_depth != depth:
          current_node = current_node.parent
          current_depth -= 1
      if depth == current_depth:
        node = TraceNode(name, data, current_node.parent)
        current_node.parent.children.append(node)
        current_node = node
      elif depth > current_depth:
        node = TraceNode(name, data, current_node)
        current_node.children.append(node)
        current_node = node
        current_depth = depth
      else:
        assert False
  # Go to the root before returning the report
  while current_depth != 0:
    current_node = current_node.parent
    current_depth -= 1
  current_node.computeChildrenSum()
  return current_node


def parseReportsInJsonFile(filename):
  """Parse all the reports in a JSON file and returns a dictionary of TraceNodes.

  Args:
    filename: File to parse

  Returns:
    a dictionary of instances of TraceNodes
  """
  result = {}
  json_content = []
  try:
    f = open(filename, "r")
    file_content = f.read()
    json_content = json.loads(file_content)
  except IOError, e:
    logger.warning("Unable to read JSON file" + filename)
  for (number, tree) in enumerate(json_content):
    result[number] = TraceNode.fromJson(tree)
    result[number].computeChildrenSum()
  return result


def parseReportsInFile(filename):
  """Parse all the reports in a file and returns a dictionnary of TraceNodes.

  Args:
    filename: File to parse

  Returns:
    a dictionnary of instances of TraceNode
  """
  parsed_reports = {}
  file_content = ''
  if filename.endswith(".json"):
    return parseReportsInJsonFile(filename)
  else:
    try:
      if filename == '-':
        file_content = sys.stdin.read()
      else:
        f = open(filename, 'r')
        file_content = f.read()
    except IOError, e:
      logger.warning("Unable to open file " + filename)
    reports = findReports(file_content)
    for process_id in reports:
      parsed_reports[process_id] = parseReport(reports[process_id])
    return parsed_reports


def parseReportsInFiles(filenames):
  """Parse reports in a list of files.

  In case of improperly formatted file, logg the exception and skip the file.

  Args:
    filenames: List of files to parse

  Returns: A dictinnary of dictionnaries of reports:
    result["filename"] = {1: Report1, ...}
  """
  result = {}
  logger.info("Parsing reports in " + str(filenames))
  for filename in filenames:
    logger.info("Parsing " + filename)
    try:
      reports = parseReportsInFile(filename)
      result[filename] = reports
    except (MalFormedFileException, AssertionError), e:
      # Catch the exception and skip to the next file
      logger.error("In " + filename + ": " + str(e))
  return result


if __name__ == "__main__":
  parsed_files = parseReportsInFiles(sys.argv[1:])
  for filename in parsed_files:
    print "\n\n-------------------\nFrom " + filename
    for report_id in parsed_files[filename]:
      print "Report #" + str(report_id)
      parsed_files[filename][report_id].prettyPrint()

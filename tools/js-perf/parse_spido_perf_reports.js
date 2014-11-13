function computeSums(node) {
	var children = node.children;
	if (node.childrenSums == null) {
		node.childrenSums = {totalTime: 0, flops: node.totalFlops,
							bytesSent: node.totalBytesSent,
							bytesReceived: node.totalBytesReceived,
							commTime: node.totalCommTime};
		for (var i = 0; i < children.length; i++) {
			var childSum = computeSums(children[i]);
			node.childrenSums.totalTime += childSum.totalTime;
			node.childrenSums.flops += childSum.flops;
			node.childrenSums.bytesSent += childSum.bytesSent;
			node.childrenSums.bytesReceived += childSum.bytesReceived;
			node.childrenSums.commTime += childSum.commTime;
		}
	}
	return node.childrenSums;
}


/* Compute various stats for a given node */
function computeData(node) {
	node.childrenSums = null;
	var children = node.children;

	if (!node.parent) {
		node.totalTime = 0;
		for (var i = 0; i < node.children.length; i++) {
			node.totalTime += node.children[i].totalTime;
		}
	}

	for (var i = 0; i < children.length; i++) {
		computeData(children[i]);
	}
	computeSums(node);

	// Percentage of time of the parent
	if (node.parent) {
		node.percentageOfParentTime = 100 * node.totalTime / node.parent.totalTime;
	} else {
		node.percentageOfParentTime = 100;
	}
	node.percentageOfTimeInChildren = 100 * node.childrenSums.totalTime / node.totalTime;
	node.percentageOfSelfTime = 100 - node.percentageOfTimeInChildren;
}

/* Parse a single JSON report */
function parseReport(report) {
	function addParent(node, parent) {
		node.parent = parent;
		node.children.forEach(function(child) {addParent(child, node)});
	}
	if (!report.parent) { 
		addParent(report);
	}
	computeData(report);
	return report;
}

/* Parse an array of JSON reports */
function parseReports(reports) {
	var result = [];
	for (var i = 0; i < reports.length; i++) {
		result.push(parseReport(reports[i]));
	}
	return result;
}

function prettyPrintValue(value, precision) {
	var suffix = "";
	if (value > 1e12) {
		value /= 1e12;
		suffix = "T";
	} else if (value > 1e9) {
		value /= 1e9;
		suffix = "G";
	} else if (value > 1e6) {
		value /= 1e6;
		suffix = "M";
	} else if (value > 1e3) {
		value /= 1e3;
		suffix = "k";
	}
	return value.toFixed(precision) + suffix;
}

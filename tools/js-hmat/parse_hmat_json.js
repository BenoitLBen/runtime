Hmat = (function () {
    var hmat = null;

    function parseHmatJson(jsonData) {
	hmat = JSON.parse(jsonData);
    }

    function findNodeAtCoords(node, x, y) {
    	function isInMe(node, x, y) {
    		var rows = node.rows, cols = node.cols;
    		if ((x >= cols.offset) && (x < cols.offset + cols.n)
    			&& (y >= rows.offset) && (y < rows.offset + rows.n)) {
    			return true;
    		} else {
    			return false;
    		}
    	}
    	if (node.isLeaf) {
    		return node;
    	} else {
    		for (var i = 0; i < node.children.length; i++) {
    			if (isInMe(node.children[i], x, y)) {
    				return findNodeAtCoords(node.children[i], x, y);
    			}
    		}
    		return null;
    	}
    }

    var svgGroup = null;
    // For the highlight
    var currentNode = null;
    var highlight = null;
    var highlightText = null;

    function zoom() {
        svgGroup.attr("transform", "translate(" + d3.event.translate + ")scale(" + d3.event.scale + ")");
    }
    var zoomListener = d3.behavior.zoom().on("zoom", zoom);
    var scale = null;

        /** Draw the HMatrix to a given element.

	The HMatrix is zoomable.

	@param elementId
	@param onMouseMove
     */
    function drawHMatrix(elementId, onMouseMove) {
	var xSize = $(elementId).width(), ySize = $(elementId).height();
	var svg = d3.select(elementId).append("svg")
	    .attr("width", xSize)
	    .attr("height", ySize)
	    .call(zoomListener);
	if (onMouseMove) {
	    svg.on("mousemove", onMouseMove);
	}
	svgGroup = svg.append("g");
	var dofCount = hmat.points.length;
	scale = .95 * Math.min(xSize, ySize) / dofCount;

	svgGroup.append("rect").attr("x", 0).attr("y", 0)
	    .attr("width", dofCount * scale)
	    .attr("height", dofCount * scale)
	    .attr("stroke-width", 1)
	    .attr("stroke", "black")
	    .attr("fill", "none");
	highlight = svgGroup.append("rect").attr("x", 0).attr("y", 0)
	    .attr("width", 0)
	    .attr("height", 0)
	    .attr("fill", "green")
	    .attr("opacity", .8);
	highlightText = svgGroup.append("text")
            .attr('x', 0)
            .attr('y', 0)
            .attr('fill', 'black')
            .attr('font-size', 0.);
	function cross(startX, startY, middleX, middleY, xLength, yLength) {
	    svgGroup.append("line")
		.attr("x1", scale * startX)
		.attr("y1", scale * middleY)
		.attr("x2", scale * (startX + xLength))
		.attr("y2", scale * middleY)
		.attr("stroke-width", 1)
		.attr("stroke", "black");
	    svgGroup.append("line")
		.attr("x1", scale * middleX)
		.attr("y1", scale * startY)
		.attr("x2", scale * middleX)
		.attr("y2", scale * (startY + yLength))
		.attr("stroke-width", 1)
		.attr("stroke", "black");
	}
	function recursiveDraw(node) {
	    if (node.isLeaf) {
		if (node.leaf_type == "Full") {
		    svgGroup.append("rect")
			.attr("x", scale * node.cols.offset)
			.attr("y", scale * node.rows.offset)
			.attr("width", scale * node.cols.n)
			.attr("height", scale * node.rows.n)
			.attr("stroke", "black")
			.attr("fill", "red");
		}
		return;
	    }
	    var startX = node.cols.offset;
	    var xLength = node.cols.n;
	    var startY = node.rows.offset;
	    var yLength = node.rows.n;
	    var middleX = node.children[2].cols.offset;
	    var middleY = node.children[1].rows.offset;
	    cross(startX, startY, middleX, middleY, xLength, yLength);
	    for (var i = 0; i < 4; i++) {
		recursiveDraw(node.children[i]);
	    }
	}
	recursiveDraw(hmat.tree);
	return svgGroup;
    }

    /** Convert the coordinates of a mouse event to the DOF space.

	@param x
	@param y
	@return
     */
    function toDofCoords(x, y) {
    	var displayTranslation = zoomListener.translate();
    	var displayScale = zoomListener.scale();
    	return [
    	    (x - displayTranslation[0]) / displayScale / scale,
    	    (y - displayTranslation[1]) / displayScale / scale];
    }

    /** Convert the coordinated from the DOF space to the figure. */
    function fromDofCoords(x, y) {
    	return [x * scale, y * scale];
    }

    var firstTime = true;
    function highlightNode(d, i, callback) {
	var coords = d3.mouse(this);
	var dofCoords = toDofCoords(coords[0], coords[1]);
	var node = findNodeAtCoords(hmat.tree, dofCoords[0], dofCoords[1]);
	if (!node || (node == currentNode)) {
	    return;
	}
	currentNode = node;
	figureCoords = fromDofCoords(node.cols.offset, node.rows.offset);
	highlight.transition(1000)
    	    .attr("x", figureCoords[0])
    	    .attr("y", figureCoords[1])
	    .attr("width", scale * node.cols.n)
	    .attr("height", scale * node.rows.n)
	    .attr("text", "Hello");
	highlightText.transition(1000).text(node.leaf_type == "Rk" ? node.k : "")
        .attr('x', figureCoords[0])
        .attr('y', figureCoords[1] + .9 * scale * node.rows.n)
        .attr('fill', 'black')
        .attr('font-size', .9 * scale * node.rows.n + "px");
	if (callback) {
	    callback(node);
	}
	$('#leaf-type').text(node.leaf_type);
	$('#rows-count').text(node.rows.n);
	$('#cols-count').text(node.cols.n);
	if (node.leaf_type == "Rk") {
    	    $('#rank').text(node.k);
    	    var compressionRatio = (node.k * (node.cols.n + node.rows.n)) / ((node.cols.n * node.rows.n));
    	    $('#compression-ratio').text((100 * compressionRatio).toFixed(3) + "%");
	} else {
    	    $('#rank').text("N/A");
    	    $('#compression-ratio').text("N/A");
	}
	if (firstTime) {
	    firstTime = false;
	    $('#legend').show();
            $('#legend').animate({opacity: 0.8}, 700, function() {});
	}
    }

    // Public functions
    return {
	parseHmatJson: parseHmatJson,
	findNodeAtCoords: findNodeAtCoords,
	drawHMatrix: drawHMatrix,
	toDofCoords: toDofCoords,
	fromDofCoords: fromDofCoords,
	highlightNode: highlightNode
    }
}());


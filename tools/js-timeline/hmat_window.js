HmatWindow = (function () {
    var svgGroup = null;
    function draw() {
	svgGroup = Hmat.drawHMatrix("#hmat-container", Hmat.highlightNode);
	highlight = svgGroup.append("rect")
    	    .attr("x", 0)
    	    .attr("y", 0)
	    .attr("width", 0)
	    .attr("height", 0)
	    .attr("stroke-width", 1)
	    .attr("stroke", "none")
	    .attr("fill", "green")
	    .attr("opacity", .7);
    }

    /** Return an HTML Table describing an HMatrix. */
    function describeHMatrix(hmat) {
	return "<p><table class='legend-table'>"
	    + "<tr><td>Type</td><td><b>" + (hmat.mode == "R" ? "READ" : "WRITE") + "</b></td></tr>"
	    + "<tr><td>Rows</td><td><b>" + hmat.rows[1] + "</b></td></tr>"
	    + "<tr><td>Cols</td><td><b>" + hmat.cols[1] + "</b></td></tr></table></p>";
    }

    var rects = [];
    function showTask(task) {
	var matrices = task.extraData;
	var legendHtml = "";
    	for (var i = 0; i < matrices.length; i++) {
	    var mat = matrices[i];
	    legendHtml += describeHMatrix(mat);
    	    if (i == rects.length) {
    		rects[i] = svgGroup.append("rect")
    		    .attr("x", 0)
    		    .attr("y", 0)
    		    .attr("width", 0)
    		    .attr("height", 0)
    		    .attr("stroke-width", 1)
    		    .attr("stroke", "none")
    		    .attr("fill", "green")
    		    .attr("opacity", .7);
    	    }
    	    var coords = Hmat.fromDofCoords(mat.cols[0], mat.rows[0]);
    	    var size = Hmat.fromDofCoords(mat.cols[1], mat.rows[1]);
    	    rects[i].transition(1000)
    		.attr("x", coords[0])
    		.attr("y", coords[1])
		.attr("width", size[0])
		.attr("height", size[1])
		.attr("fill", mat.mode == "R" ? "blue" : "purple");
    	}
	for (var i = matrices.length; i < rects.length; i++) {
	    rects[i].transition(1000)
    		.attr("x", 0)
    		.attr("y", 0)
		.attr("width", 0)
		.attr("height", 0);
	}
	$("#legend").html(legendHtml);
	if (!$("#legend").is(":visible")) {
	    $("#legend").show();
	    $('#legend').animate({opacity: 0.8}, 700, function() {});
	}
    }

    return {
	draw: draw,
	showTask: showTask
    }
}());

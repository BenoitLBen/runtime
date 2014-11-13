function parseTimeline(timelineData) {
    data = JSON.parse(timelineData);
    return data;
    $('#file-selector').transition(1000).attr("opacity", 0.);
}

function prettyPrintDuration(duration) {
    if (duration < 1e3) {
	return (duration).toFixed(2) + "ns";
    } else if (duration < 1e6) {
	return (duration * 1e-3).toFixed(2) + "us";
    } else if (duration < 1e9) {
	return (duration * 1e-6).toFixed(2) + "ms";
    } else {
	return (duration * 1e-9).toFixed(2) + "s";
    }
}

function drawTimeline(timeline) {
    var colors = ["green", "red", "blue", "gold",
		  "purple", "darksalmon", "darkolivegreen", "dodgerblue" ];
    var colorIndex = 0;
    var taskToColor = {};

    function toItems(timeline) {
	var items = [];
	for (var i = 0; i < timeline.length; i++) {
	    var worker = timeline[i];
	    for (var j = 0; j < worker.length; j++) {
		var task = worker[j];
		var item = {worker: i, start: task.start, stop: task.stop, name: task.name};
		if (task.extraData) {
		    item.extraData = task.extraData;
		}
		items.push(item);
		if (taskToColor[task.name] === undefined) {
		    taskToColor[task.name] = colors[colorIndex % colors.length];
		    colorIndex += 1;
		}
	    }
	}
	return items;
    }

    function minMaxTime(timeline) {
	var result = [Infinity, 0];
	for (var i = 0; i < timeline.length; i++) {
	    var worker = timeline[i];
	    result[0] = Math.min(result[0], worker[0].start);
	    result[1] = Math.max(result[1], worker[worker.length - 1].stop);
	}
	return result;
    }

    function drawItemsCanvas(context, xScale, yScale) {
	for (var i = 0; i < items.length; i++) {
	    context.beginPath();
	    var task = items[i];
	    context.rect(scale * xScale(task.start), scale * yScale(task.worker),
			 scale * xScale(task.stop - task.start), .8 * scale * yScale(1));
	    context.fillStyle = taskToColor[task.name];
	    context.fill();
	    context.closePath();
	}
    }

    var items = toItems(timeline);

    var workerCount = timeline.length;
    var timeRange = minMaxTime(timeline);
    console.log(timeRange);

    var viewerWidth = $(document).width();
    var viewerHeight = $(document).height();
    var margins = {top: 20, right: 15, bottom: 15, left: 120};
    var height = viewerHeight - margins.top - margins.bottom;
    var width = viewerWidth - margins.left - margins.right;

    var miniHeight = workerCount * 12 + 50;
    var mainHeight = height - miniHeight - 50;

    var x = d3.scale.linear().domain(timeRange).range([0, width]);
    var x1 = d3.scale.linear().range([0, width]);
    var y1 = d3.scale.linear().domain([0, workerCount]).range([0, mainHeight]);
    var y2 = d3.scale.linear().domain([0, workerCount]).range([0, miniHeight]);

    var canvas = d3.select('body')
	.append('canvas')
	.attr("width", viewerWidth)
	.attr("height", viewerHeight)
	.style("position", "absolute")
	.style("top", mainHeight + margins.top + "px")
	.style("left", margins.left + "px")
	.attr("class", "chart")
	.node().getContext('2d');

    var scale = 2; // Take care of the Hi-DPI screens (MBP retina for instance)
    canvas.canvas.width = scale * viewerWidth;
    canvas.canvas.height = scale * viewerHeight;
    canvas.canvas.style.width = viewerWidth;
    canvas.canvas.style.height = viewerHeight;

    var chart = d3.select("body").append("svg")
	.attr("width", viewerWidth)
	.attr("height", viewerHeight)
	.attr("class", "chart");

    chart.append("defs").append("clipPath")
	.attr("id", "clip")
	.attr("width", width)
	.attr("height", mainHeight);

    var main = chart.append("g")
	.attr("transform", "translate(" + margins.left + "," + margins.top + ")")
	.attr("width", width)
	.attr("height", mainHeight)
	.attr("class", "main");

    var mini = chart.append("g")
	.attr("transform", "translate(" + margins.left + "," + (mainHeight + margins.top) + ")")
	.attr("width", width)
	.attr("height", miniHeight)
	.attr("class", "mini");

    drawItemsCanvas(canvas, x, y2);
    var itemRects = main.append("g")
        .attr("clip-path", "url(#clip)");

    var brush = d3.svg.brush().x(x).on("brush", updateZoomedView);
    mini.append("g")
	.attr("class", "x brush")
	.call(brush)
	.selectAll("rect")
	.attr("y", 1)
	.attr("height", miniHeight - 1);

    drawItemsCanvas(canvas, x, y2);
    updateZoomedView();

    function updateZoomedView() {
	var rects,
	    minExtent = brush.extent()[0],
	    maxExtent = brush.extent()[1],
	    visItems = items.filter(function(d) {
		return d.start < maxExtent && d.stop > minExtent;
	    });

	mini.select(".brush").call(brush.extent([minExtent, maxExtent]));
	console.log("Hello: " + minExtent + " " + maxExtent);
	console.log(visItems.length);

	x1.domain([minExtent, maxExtent]);
	main.selectAll("rect").remove();
	main.append("g").selectAll("mainItems")
	    .data(visItems)
	    .enter().append("rect")
	    .attr("x", function(d) {return x1(d.start);})
	    .attr("y", function(d) {return y1(d.worker) + 10;})
	    .attr("width", function(d) {return x1(d.stop) - x1(d.start);})
	    .attr("height", function(d) {return .8 * y1(1);})
	    .attr("fill", function(d) {return taskToColor[d.name];})
	    .on("mouseenter", function(d) {
		if (!$('#legend').is(":visible")) {
		    $('#legend').show();
		    $('#legend').animate({opacity: 0.8}, 700, function() {});
		}
		$("#task-name").text(d.name);
		$("#exec-time").text(prettyPrintDuration(d.stop - d.start));
		console.log(d);
		if (d.extraData) {
		    hmatWindow.HmatWindow.showTask(d);
		}
	    });
	var axis = d3.svg.axis().scale(x1)
	    .tickFormat(function(d) {return (d / 1e9).toFixed(2) + "s"});
	main.selectAll("svg").remove();
	main.append("svg").call(axis);
    }
}

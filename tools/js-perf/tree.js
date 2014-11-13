var PerfTree = function(){
    var highlightedNode = null;
    // Calculate total nodes, max label length
    var totalNodes = 0;
    var maxLabelLength = 0;
    // Misc. variables
    var i = 0;
    var duration = 750;
    var root;

    // size of the diagram
    var viewerWidth = $(document).width();
    var viewerHeight = $(document).height();

    var tree = d3.layout.tree()
        .separation(function(a, b) { return 3; });
        //.size([viewerHeight, viewerWidth]);

    // define a d3 diagonal projection for use by the node paths later on.
    var diagonal = d3.svg.diagonal()
        .projection(function(d) {
            return [d.x, d.y];
        });

    // A recursive helper function for performing some setup by walking through all nodes
    function visit(parent, visitFn, childrenFn) {
        if (!parent) return;

        visitFn(parent);

        var children = childrenFn(parent);
        if (children) {
            var count = children.length;
            for (var i = 0; i < count; i++) {
                visit(children[i], visitFn, childrenFn);
            }
        }
    }

    // Define the zoom function for the zoomable tree

    function zoom() {
        svgGroup.attr("transform", "translate(" + d3.event.translate + ")scale(" + d3.event.scale + ")");
    }


    // define the zoomListener which calls the zoom function on the "zoom" event constrained within the scaleExtents
    var zoomListener = d3.behavior.zoom().scaleExtent([0.1, 3]).on("zoom", zoom);

    // define the baseSvg, attaching a class for styling and the zoomListener
    var baseSvg = d3.select("#tree-container").append("svg")
        .attr("width", viewerWidth)
        .attr("height", viewerHeight)
        .attr("class", "overlay")
        .call(zoomListener);

    // Helper functions for collapsing and expanding nodes.
    function collapse(d) {
        if (d.children) {
            d._children = d.children;
            d._children.forEach(collapse);
            d.children = null;
        }
    }

    function expand(d) {
        if (d._children) {
            d.children = d._children;
            d.children.forEach(expand);
            d._children = null;
        }
    }

    // Function to center node when clicked/dropped so node doesn't get lost when collapsing/moving with large amount of children.
    function centerNode(source) {
        scale = zoomListener.scale();
        x = -source.x0;
        y = -source.y0;
        x = x * scale + viewerWidth / 2;
        y = y * scale + viewerHeight / 2;
        d3.select('g').transition()
            .duration(duration)
            .attr("transform", "translate(" + x + "," + y + ")scale(" + scale + ")");
        zoomListener.scale(scale);
        zoomListener.translate([x, y]);
    }

    // Toggle children function
    function toggleChildren(d) {
        if (d.children) {
            d._children = d.children;
            d.children = null;
        } else if (d._children) {
            d.children = d._children;
            d._children = null;
        }
        return d;
    }

    function recursivelyHideChildren(d) {
	if (d.children) {
	    d._children = d.children;
	    d.children = null;
	    for (var i = 0; i < d._children.length; i++) {
		recursivelyHideChildren(d._children[i]);
	    }
	}
	return d;
    }

    // Toggle children on click.
    function click(d) {
        if (d3.event.defaultPrevented) return; // click suppressed
        d = toggleChildren(d);
        update(d);
    }

    function update(source) {
        // Color range for the nodes.
        var color = d3.scale.linear().domain([0, 100]).range(["blue", "red"]);
        // Compute the new height, function counts total children of root node and sets tree height accordingly.
        // This prevents the layout looking squashed when new nodes are made visible or looking sparse when nodes are removed
        // This makes the layout more consistent.
        radius = 20;
        var levelWidth = [1];
        var childCount = function(level, n) {

            if (n.children && n.children.length > 0) {
                if (levelWidth.length <= level + 1) levelWidth.push(0);

                levelWidth[level + 1] += n.children.length;
                n.children.forEach(function(d) {
                    childCount(level + 1, d);
                });
            }
        };
        childCount(0, root);
        var newWidth = Math.min(d3.max(levelWidth) * radius * 4, 20 * viewerWidth);
        var newHeight = Math.min(levelWidth.length * 100, 3 * viewerHeight);
        tree = tree.size([newWidth, newHeight]).separation(function(a, b) {return 3;});

        // Compute the new tree layout.
        var nodes = tree.nodes(root).reverse(),
            links = tree.links(nodes);

        // Update the nodes…
        node = svgGroup.selectAll("g.node")
            .data(nodes, function(d) {
                return d.id || (d.id = ++i);
            });

        // Enter any new nodes at the parent's previous position.
        var nodeEnter = node.enter().append("g")
            .attr("class", "node")
            .attr("transform", function(d) {
                return "translate(" + source.x0 + "," + source.y0 + ")";
            })
            .on('click', click);

        nodeEnter.append("circle")
            .attr('class', 'nodeCircle')
            .attr("r", 0)
            .style("fill", function(d) {
                // return d._children ? "lightsteelblue" : "#fff";
                return color(d.percentageOfParentTime);
            });
        nodeEnter.append("svg:title").text(function(d) {return d["name"]});
        nodeEnter.append("text")
            .attr("x", function(d) {
                return 0; //d.children || d._children ? -20 : 20;
            })
            .attr("dy", ".35em")
            .attr('class', 'nodeText')
            .attr("text-anchor", function(d) {
                return "middle";
            })
            .text(function(d) {
                return d.totalTime.toFixed(2);
            })
            .style("fill-opacity", 0);

        // phantom node to give us mouseover in a radius around it
        nodeEnter.append("circle")
            //            .attr('class', 'ghostCircle')
            .attr("r", radius * 1.5)
            .attr("opacity", 0) // change this to zero to hide the target area
            .style("fill", function(node) {
                 return color(node.percentageOfParentTime);
            })
            .on("mouseover", function(node) {
                var firstTime = false;
                if (highlightedNode) {
                    highlightedNode.transition().duration(300).style("opacity", 0);
                } else {
                    firstTime = true;
                }
                highlightedNode = d3.select(this);
                d3.select(this).transition().duration(300).style("opacity", .5);

                $('#node-name').text(node.name);
                $('#node-total-time').text(node.totalTime.toFixed(3));
                $('#node-children-time').text(node.childrenSums.totalTime.toFixed(3));
                $('#node-children-percentage').text(node.percentageOfTimeInChildren.toFixed(2) + "%");
                $('#node-parent-percentage').text(node.percentageOfParentTime.toFixed(2) + "%");
                $('#node-calls-count').text(node.n);
                $('#node-flops').text(prettyPrintValue(node.childrenSums.flops, 2) + " (" + prettyPrintValue(node.childrenSums.flops / node.totalTime, 2) + "Flops)");
                $('#node-bytes-sent').text(node.childrenSums.bytesSent);
                $('#node-bytes-received').text(node.childrenSums.bytesReceived);
                $('#node-comm-time').text(node.childrenSums.commTime);

                if (firstTime) {
                    $('#legend').show();
                    $('#legend').animate({opacity: 0.8}, 700, function() {});
                }
            });

        // Change the circle fill depending on whether it has children and is collapsed
        node.select("circle.nodeCircle")
            .attr("r", 20)
            .style("fill", function(d) {
                return color(node.percentageOfParentTime);
                // return d._children ? "lightsteelblue" : "#fff";
            })
            .style("stroke", function(node) {
                return node._children ? "red" : "black";
            });

        // Transition nodes to their new position.
        var nodeUpdate = node.transition()
            .duration(duration)
            .attr("transform", function(d) {
                return "translate(" + d.x + "," + d.y + ")";
            });

        // Fade the text in
        nodeUpdate.select("text")
            .style("fill-opacity", 1);

        // Transition exiting nodes to the parent's new position.
        var nodeExit = node.exit().transition()
            .duration(duration)
            .attr("transform", function(d) {
                return "translate(" + source.x + "," + source.y + ")";
            })
            .remove();

        nodeExit.select("circle")
            .attr("r", 0);

        nodeExit.select("text")
            .style("fill-opacity", 0);

        // Update the links…
        var link = svgGroup.selectAll("path.link")
            .data(links, function(d) {
                return d.target.id;
            });

        // Enter any new links at the parent's previous position.
        link.enter().insert("path", "g")
            .attr("class", "link")
            .attr("d", function(d) {
                var o = {
                    x: source.x0,
                    y: source.y0
                };
                return diagonal({
                    source: o,
                    target: o
                });
            });

        // Transition links to their new position.
        link.transition()
            .duration(duration)
            .attr("d", diagonal);

        // Transition exiting nodes to the parent's new position.
        link.exit().transition()
            .duration(duration)
            .attr("d", function(d) {
                var o = {
                    x: source.x,
                    y: source.y
                };
                return diagonal({
                    source: o,
                    target: o
                });
            })
            .remove();

        // Stash the old positions for transition.
        nodes.forEach(function(d) {
            d.x0 = d.x;
            d.y0 = d.y;
        });

        if (newWidth > viewerWidth || newHeight > viewerHeight) {
            var newScale = Math.min(viewerWidth / newWidth, viewerHeight / newHeight);
            d3.select('g').transition()
            .duration(1500)
            .attr("transform", "scale(" + newScale + ")");
            zoomListener.scale(newScale);
            centerNode(source);
        }
        if (newWidth < .1 * viewerWidth || newHeight < .1 * viewerHeight) {
            var newScale = Math.min(.5 * viewerWidth / newWidth, .5 * viewerHeight / newHeight);
            d3.select('g').transition()
            .duration(1500)
            .attr("transform", "scale(" + newScale + ")");
            zoomListener.scale(newScale);
            centerNode(source);
        }
    }

    // Append a group which holds all nodes and which the zoom Listener can act upon.
    var svgGroup = baseSvg.append("g");

    // Public functions
    return {
        showReport: function(report) {
            root = report;
            root.x0 = viewerHeight / 2;
            root.y0 = 0;
            // Layout the tree initially and center on the root node.
            update(root);
        },
        toggleFirstLevel: function() {
            root.children.forEach(function(child) {
		recursivelyHideChildren(child);
                // toggleChildren(child);
            });
            update(root);
        }
    }
}();


var Reports = function() {
    var reports = null;

    function loadReports(fileContent) {
        var jsonData = JSON.parse(fileContent);
        reports = parseReports(jsonData);
        var selectMenu = document.getElementById("report-selector");
        for (var i = 0; i < reports.length; i++) {
            var option = document.createElement("option");
            option.text = reports[i].name;
            selectMenu.add(option);
        }
        $('#report-selector-box').show().animate({opacity: 0.8}, 700, function() {});
        PerfTree.showReport(reports[0]);
    }

    function openReportsFile(e) {
        var file = e.target.files[0];
        if (!file) {
           return;
        }
        var reader = new FileReader();
        reader.onload = function (e) {
            var contents = e.target.result;
            loadReports(contents);
            $('#file-selector-box').animate({opacity: 0}, 700,
                function() {$('#file-selector-box').hide()});
        }
        reader.readAsText(file);
    }
    document.getElementById('file-input')
        .addEventListener('change', openReportsFile, false);

    return {
        selectReport: function(index) {
            var selectBox = document.getElementById("report-selector");
            PerfTree.showReport(reports[selectBox.selectedIndex]);
        }
    }
}()

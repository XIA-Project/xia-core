function trim (str) {
    return str.replace(/^\s\s*/, '').replace(/\s\s*$/, '');
}

function measureText(pText) {
    var lDiv = document.createElement('lDiv');

    document.body.appendChild(lDiv);

    //lDiv.style.fontSize = "" + pFontSize + "px";
    lDiv.style.position = "absolute";
    lDiv.style.left = -1000;
    lDiv.style.top = -1000;

    lDiv.innerHTML = pText;

    var lResult = {
        width: lDiv.clientWidth,
        height: lDiv.clientHeight
    };

    document.body.removeChild(lDiv);
    lDiv = null;

    return lResult;
}

function drawFilledPolygon(canvas,shape)/*{{{*/
{
	canvas.beginPath();
	canvas.moveTo(shape[0][0],shape[0][1]);

	for(p in shape)
		if (p > 0) canvas.lineTo(shape[p][0],shape[p][1]);

	canvas.lineTo(shape[0][0],shape[0][1]);
	canvas.fill();
};

function translateShape(shape,x,y)
{
	var rv = [];
	for(p in shape)
		rv.push([ shape[p][0] + x, shape[p][1] + y ]);
	return rv;
};

function rotateShape(shape,ang)
{
	var rv = [];
	for(p in shape)
		rv.push(rotatePoint(ang,shape[p][0],shape[p][1]));
	return rv;
};

function rotatePoint(ang,x,y)
{
	return [
		(x * Math.cos(ang)) - (y * Math.sin(ang)),
		(x * Math.sin(ang)) + (y * Math.cos(ang))
	];
};

function drawLineArrow(canvas,x1,y1,x2,y2)
{
	canvas.beginPath();
	canvas.moveTo(x1,y1);
	canvas.lineTo(x2,y2);
	canvas.stroke();
	var ang = Math.atan2(y2-y1,x2-x1);
	drawFilledPolygon(canvas,translateShape(rotateShape(arrow_shape,ang),x2,y2));
};

var arrow_shape = [
	[ -10, -4 ],
	[ -8, 0 ],
	[ -10, 4 ],
	[ 0, 0 ]
];


function Node(label) {
	this.x = 0;
	this.y = 0;
	this.radius = 25;
	this.label = label;
	this.outEdges = new Array();
	this.lastShiftId = 0;
	this.labelYOffset = 0;
}

Node.prototype.draw = function(context) {
	context.beginPath();
	context.arc(this.x,this.y,this.radius,0,Math.PI*2,true);
	context.closePath();
	context.stroke();
	
	context.textBaseline = 'top';
	var labelSize = measureText(this.label);
	var labelX = (this.x+this.radius)-labelSize.width/2;
	var labelY = this.y+this.labelYOffset;
	context.fillStyle = '#fff';
	context.fillRect(labelX, labelY, labelSize.width, labelSize.height);
	context.fillStyle = '#000';
	context.fillText(this.label, labelX, labelY);
}

Node.prototype.shift = function(x, y, id) {
	if (this.lastShiftId != id)
	{
		this.lastShiftId = id;
		if (this.processed()) {
			this.x += x;
			this.y += y;
		}
		
		for (var i = 0; i < this.outEdges.length; i++) {
			if (this.outEdges[i] == this) continue; // to avoid problems with looping on the last node
			this.outEdges[i].shift(x, y, id);
		}
	}
}

Node.prototype.drawOutEdges = function(context) {
	for (var i = 0; i < this.outEdges.length; i++) {
		var child = this.outEdges[i];
		
		// calculate line endpoints (they should be on node circles, not centers)
		var xDiff = Math.abs(this.x - child.x);
		var yDiff = Math.abs(this.y - child.y);
		var distance = Math.sqrt(xDiff*xDiff + yDiff*yDiff);
		
		var xOffsetThis = (xDiff * this.radius) / distance;
		var yOffsetThis = (yDiff * this.radius) / distance;
		var xOffsetChild = (xDiff * child.radius) / distance;
		var yOffsetChild = (yDiff * child.radius) / distance;
		
		var thisX = this.x;
		var thisY = this.y;
		var childX = child.x;
		var childY = child.y;
		
		if (this.x > child.x) {
			thisX -= xOffsetThis;
			childX += xOffsetChild;
		} else {
			thisX += xOffsetThis;
			childX -= xOffsetChild;
		}
		if (this.y > child.y) {
			thisY -= yOffsetThis;
			childY += yOffsetChild;
		} else {
			thisY += yOffsetThis;
			childY -= yOffsetChild;
		}
		
		// Draw the line
		context.moveTo(thisX, thisY);
		context.lineTo(childX, childY);
		context.stroke();
		
		// Draw the arrowhead
		var ang = Math.atan2(childY-thisY,childX-thisX);
		drawFilledPolygon(context,translateShape(rotateShape(arrow_shape,ang),childX,childY));
	}
}

Node.prototype.processed = function() {
	return (!(this.x == 0 && this.y == 0));
}


function StartNode() {
	this.radius = 10;
}

StartNode.prototype = new Node(""); // subclass of Node

StartNode.prototype.draw = function(context) {  // override the draw method
	context.beginPath();
	context.arc(this.x,this.y,this.radius,0,Math.PI*2,true);
	context.closePath();
	context.fill();
}



function drawDAG(DAG, canvas) {
	// resize inspector (because the CSS scales it)
	canvas.width = window.innerWidth;
	canvas.height = 400
	
	var context=canvas.getContext("2d");
	var startX = 15;
	var startY = 30;
	var hspace = 200;
	var vspace = 75;
	var shiftId = 0;
	
	DAG = DAG.replace(/(\r\n|\n|\r)/gm,"");
	var nodeTextArray = DAG.split("-");
	if (nodeTextArray[0].split(" ")[0] != "DAG")
		return;
	
	// Make an array of node objects indexed by node number
	// e.g.: DAG 0
	//       CID:blah  ---> goes in nodeArray[0]
	var nodeArray = new Array();
	//var startingNodeIndices = new Array(); // The indices of the outgoing nodes from the implicit starting node
	var startNode;
	//nodeTextArray[0] just says "dag"
	//nodeTextArray[1] contains the out edges for the starting node
	startNode = new StartNode();
	startNode.outEdges = nodeTextArray[0].trim().split(" ").slice(1); // slice(1) takes a subarray starting at 1
	// the rest of the entries in nodeTextArray are regular DAG nodes
	for (var i=1; i < nodeTextArray.length; i++) {  // entry 0 is the starting node out edges, so start at 1
		var line = nodeTextArray[i].trim()
		//var nodeInfo = nodeTextArray[i].split("=");
		var nodeLabel = line.split(" ")[0]  //nodeInfo[0];
		var nodeNum = i-1; //nodeInfo[1].split(":")[0];

		var node = new Node(nodeLabel);
		node.outEdges = line.split(" ").slice(1);  //nodeInfo[1].split(":")[1].split(",");
		nodeArray[nodeNum] = node;
	}

	
	// Replace indices in each node's outbound edges list with the nodes themselves
	// startNode
	for (var j = 0; j < startNode.outEdges.length; j++) {
		startNode.outEdges[j] = nodeArray[startNode.outEdges[j]];
	}
	// other nodes
	for (var i = 0; i < nodeArray.length; i++) {
		var node = nodeArray[i];
		
		for (var j = 0; j < node.outEdges.length; j++) {
			node.outEdges[j] = nodeArray[node.outEdges[j]];
		}
	}

	
	// Process & draw nodes in a BFS fashion
	// Make an empty queue
	var nodesToDraw = new Array();
	
	// Initialize the queue with the starting node
	startNode.x = startX;
	startNode.y = startY;
	nodesToDraw.push(startNode);
	
	
	// Now set each node's position
	while (nodesToDraw.length > 0) {
		var node = nodesToDraw.shift();
		var nextY = node.y;
		
		// process children
		for (var i = 0; i < node.outEdges.length; i++) {
			var child = node.outEdges[i];
			if (child == node) continue; // to avoid problems with looping on the last node
			
			if (!child.processed())
			{
				child.x = node.x + hspace;
				child.y = nextY;
				child.labelYOffset = 10*((child.x-startNode.x)/hspace - 1); // try to stagger labels vertically based on how many nodes are already in this row
				nextY += vspace;
				nodesToDraw.push(child);
			} 
			else if (child.x <= node.x) 
			{
				child.shift((node.x - child.x + hspace), 0, shiftId);
				shiftId++;
				nextY = child.y + vspace;
			}
		}
	}
	
	
	// Now that the positions have been set, draw the nodes and their edges
	startNode.draw(context);
	startNode.drawOutEdges(context);
	for (var i = 0; i < nodeArray.length; i++) {
		var node = nodeArray[i];
		node.draw(context);
		node.drawOutEdges(context);
	}
}

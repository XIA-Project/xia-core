﻿package edu.cmu.networkelements {	import flash.display.Shape;	import flash.display.Sprite;	import flash.display.MovieClip;	import flash.geom.Point;	import flash.text.*;	import flash.utils.Dictionary;	import flash.events.MouseEvent;	import edu.cmu.MyGlobal	import edu.cmu.gui.*;		public class Connection {				// Constants:		// Public Properties:		// Private Properties:		private var element1:NetworkElement;		private var element2:NetworkElement;		private var element1Port:int;		private var element2Port:int;				private var line:Shape;		private var trafficLines:Vector.<Shape>;		private var point1:Point;		private var point2:Point;				private var infoPanel:LinkInfoBase;				private var currentTrafficByElementID:Dictionary; // key: element name; value: dictionary (key: principal type; value: pkts/sec)		private var packetCountLabels:Dictionary; // key: principal type; value: TextField displaying packet count			// Initialization:		public function Connection(element1:NetworkElement, element1Port:int, element2:NetworkElement, element2Port:int) { 			this.element1 = element1;			this.element2 = element2;			this.element1Port = element1Port;			this.element2Port = element2Port;						currentTrafficByElementID = new Dictionary();			trafficLines = new Vector.<Shape>();						// set starting link info panel type			this.setInfoPanelType(MyGlobal.startingLinkInfoViewType);		}			// Public Methods:		public function GetElement1ID():String {			return element1.GetElementID();		}				public function GetElement2ID():String {			return element2.GetElementID();		}				public function GetRemoteElementID(localElement:NetworkElement):String {			if (element1.GetElementID() == localElement.GetElementID()) {				return element2.GetElementID();			} else {				return element1.GetElementID();			}		}				public function GetRemoteElementPort(localElement:NetworkElement):int {			if (element1.GetElementID() == localElement.GetElementID()) {				return element2.GetLocalPortForElement(localElement);			} else {				return element1.GetLocalPortForElement(localElement);			}		}				public function RemoveConnection():void {			if (element1 != null && element1.parent != null) {				if (line != null && element1.parent.contains(line)) {					element1.parent.removeChild(line);				}				if (infoPanel != null && element1.parent.contains(infoPanel)) {					element1.parent.removeChild(infoPanel);				}				for (var i:int; i < trafficLines.length; i++) {					if (element1.parent.contains(trafficLines[i])) {						element1.parent.removeChild(trafficLines[i]);					}				}			} else if (element2 != null && element2.parent != null) {				if (line != null && element2.parent.contains(line)) {					element2.parent.removeChild(line);				}				if (infoPanel != null && element2.parent.contains(infoPanel)) {					element2.parent.removeChild(infoPanel);				}				for (var i:int; i < trafficLines.length; i++) {					if (element2.parent.contains(trafficLines[i])) {						element2.parent.removeChild(trafficLines[i]);					}				}			}									trafficLines = new Vector.<Shape>();						element1.RemoveConnectionFromPort(element1Port);			element2.RemoveConnectionFromPort(element2Port);		}				// Takes a dictionary mapping XIDType:String -> Pkts/sec:int and a sending network element		public function UpdateTraffic(sender:NetworkElement, currentRates:Dictionary):void {			currentTrafficByElementID[sender.GetElementID()] = currentRates;						if (infoPanel != null) {				infoPanel.updateCurrentTraffic(AggregateTraffic());			}						Redraw();		}				public function Redraw():void {			// make sure info panel is displayed			if (infoPanel != null && 				!element1.parent.contains(infoPanel)) {				element1.parent.addChild(infoPanel);			}						// remove old lines			if (line != null) {				element1.parent.removeChild(line);				line = null;			}			for (var i:int; i < trafficLines.length; i++) {				if (element1.parent.contains(trafficLines[i])) {					element1.parent.removeChild(trafficLines[i]);				}			}			trafficLines = new Vector.<Shape>();						// create new line			line = new Shape();						// define the line style			line.graphics.lineStyle(3,0x666666, 0.8);						// set the starting point for the line			point1 = element2.GetEdgePointNearestPoint(element1.GetCenterPoint());			line.graphics.moveTo(point1.x, point1.y);			 			// move the line to endpoint			point2 = element1.GetEdgePointNearestPoint(element2.GetCenterPoint());			line.graphics.lineTo(point2.x, point2.y);												// Create colored lines representing current traffic on this connection			var aggregateCounts:Dictionary = AggregateTraffic();			var widthSoFar = 3; // just the normal line			for (var type:String in aggregateCounts) {				var trafficLine:Shape = new Shape();								var widthIncrease:int = Math.ceil(Math.sqrt(aggregateCounts[type]));												if (widthIncrease > 0) {					widthSoFar += widthIncrease;					trafficLine.graphics.lineStyle(widthSoFar, MyGlobal.getColorForPrincipal(type), 1);					trafficLine.graphics.moveTo(point1.x, point1.y);					trafficLine.graphics.lineTo(point2.x, point2.y);					trafficLines.push(trafficLine);				}											}						// add line to the stage			element1.parent.addChild(line);			element1.parent.setChildIndex(line, 0);						// add colored traffic category lines (in reverse order			// because we push each one to the back after adding)			//for (var i:int = trafficLines.length - 1; i >= 0; i--) {  // Need a separate loop so biggest line is on the bottom			for (var i:int = 0; i < trafficLines.length; i++) {  // Need a separate loop so biggest line is on the bottom				element1.parent.addChild(trafficLines[i]);				element1.parent.setChildIndex(trafficLines[i], 0);			}						// move info panel			this.PositionInfoPanel();						// move elements back to top so labels are on top of link line			//element1.moveToTop();			//element2.moveToTop();		}				public function setInfoPanelType(type:String):void {			if (type == "ChartStandard") {				infoPanel = new edu.cmu.gui.LinkInfoChartStandard();			} else if (type == "ChartCompact") {				infoPanel = new edu.cmu.gui.LinkInfoChartCompact();			} else if (type == "Panel") {				infoPanel = new edu.cmu.gui.LinkInfoPanel();			} else if (type == "Stub") {				//if (infoPanel != null &&//					element1.parent.contains(infoPanel)) {//					element1.parent.removeChild(infoPanel);//				}//				infoPanel = null;//				return;				infoPanel = new edu.cmu.gui.LinkInfoStub();			}			infoPanel.addEventListener(MouseEvent.CLICK, infoPanelClicked);		}				// Protected Methods:				protected function PositionInfoPanel():void {						if (infoPanel == null) {return;}						// If slope of line is positive, put panel below and to the right.			// If slope of line is negative, put panel above and to the right.			// (The code seems backwards because the origin is in the upper left.)			if ((point1.y - point2.y) / (point1.x - point2.x) > 0) {				var point:Point = Point.interpolate(point1, point2, 0.5);  // panel starts above line's midpoint				infoPanel.x = point.x;				infoPanel.y = point.y - infoPanel.height;			} else {				var point:Point = Point.interpolate(point1, point2, 0.5);  // panel starts at line's midpoint				infoPanel.x = point.x;				infoPanel.y = point.y;			}						// Make sure the infoPanel is always on top			// FIXME: this causes pairs of charts to compete with each other; they alternate who's on top			//infoPanel.parent.setChildIndex(infoPanel, infoPanel.parent.numChildren - 1);		}				private function AggregateTraffic():Dictionary {			var aggregateCounts:Dictionary = new Dictionary();			var element1Counts:Dictionary = currentTrafficByElementID[element1.GetElementID()];			var element2Counts:Dictionary = currentTrafficByElementID[element2.GetElementID()];						// Put element 1's counts in aggregateCounts			for (var type:String in element1Counts) {				aggregateCounts[type] = element1Counts[type];			}						// Now add element 2's counts to element 1's			for (var type:String in element2Counts) {				if (aggregateCounts[type] == null) {					aggregateCounts[type] = 0;				}								aggregateCounts[type] += element2Counts[type];			}						return aggregateCounts;		}				private function infoPanelClicked(event:MouseEvent):void {			if (element1.parent.contains(infoPanel)) {				element1.parent.removeChild(infoPanel);			}						if (infoPanel is edu.cmu.gui.LinkInfoChartStandard) {				this.setInfoPanelType("ChartCompact");			} else if (infoPanel is edu.cmu.gui.LinkInfoChartCompact) {				this.setInfoPanelType("Panel");			} else if (infoPanel is edu.cmu.gui.LinkInfoPanel) {				this.setInfoPanelType("Stub");			} else {				this.setInfoPanelType("ChartStandard")			}					}					}}
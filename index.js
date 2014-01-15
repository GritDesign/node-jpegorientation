
exports.orientation = require("./build/Release/jpegexiforient.node").orientation;
exports.autoRotate = require("./lib/autoRotate").autoRotate;

exports.orientationToString = function(o) {
	switch (o) {
		case 1: return "TopLeft";
		case 2: return "TopRight";
		case 3: return "BottomRight";
		case 4: return "BottomLeft";
		case 5: return "LeftTop";
		case 6: return "RightTop";
		case 7: return "RightBottom";
		case 8: return "LeftBottom";
	}
}

var orientation = require("../build/Release/jpegexiforient");
var gm = require("gm");


function autoRotate(source, destination, cb) {
	console.log("Checking " + source);
	orientation.orientation(source, function(err, val) {
		if (err) return cb(err);
		var img = gm(source);
		switch (val) {
			case 1: 	//	top-left  - no transform
				break;
			case 2: 	//	top-right - flip horizontal
				img = img.flop(); 
				break;
			case 3: 	//	bottom-right - rotate 180
				img = img.rotate("black", 180);
				break;
			case 4: 	//	bottom-left - flip vertically
				img = img.flip();
				break;
			case 5: 	//	left-top - rotate 90 and flip horizontal
				img = img.rotate("black", 90).flop();
				break;
			case 6: 	//	right-top - rotate 90
				img = img.rotate("black", 90);
				break;
			case 7: 	//	right-bottom - rotate 270 and flip horizontal
				img = img.rotate("black", 270).flop();
				break;
			case 8: 	//	left-bottom - rotate 270
				img = img.rotate("black", 270);
				break;
		}
		img.write(destination, function(err) {
			orientation.orientation(destination, 1, cb);
		});
	});
}

exports.autoRotate = autoRotate;
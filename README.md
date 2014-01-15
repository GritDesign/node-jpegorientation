# node-jpegorientation v1.0.0
Tool for querying, setting and correcting orientation of JPEG files in node.js.

## Basic Usage

Get the orientation for an image file:

```
var jpeg = require("jpegorientation");
jpeg.orientation("IMG_0001.jpg", function(err, orientation) {
	console.log(jpeg.orientationToString(orientation)); // --> "TopLeft"
}

```

Auto Rotate an image:

```
var jpeg = require("jpegorientation");
jpeg.autoRotate("IMG_0001.jpg", function(err) {
	
}

```


Manually set the Exif data for a files orientation (does not actually rotate, just modifies the metadata):

```
var jpeg = require("jpegorientation");
jpeg.orientation("IMG_0001.jpg", 5, function(err, orientation) {
	
}


```




## Installation

    npm install jpegorientation
    
## API Overview

### Get or Set JPEG orientation EXIF data

Gets or sets the "Orientation" flag in the specified JPEG's EXIF data section.  This requires the EXIF section to be defined in the JPEG files.

```
jpeg.orientation(source, [newOrientationValue], callback);
```

*note: This does NOT rotate the image, only changes the data in the EXIF header of the image (See autoRotate)*


### Auto Rotate

Automatically rotate the image to the correct "TopLeft" (orientation=1) and save the orientation data in the Exif headers.

```
jpeg.autoRotate(source, destination, callback);
```

### Orientation to String

Returns a friendly string describing the orientation.  Return value is position for X then Y of the image.  (e.g. TopLeft, RightBottom)

```
jpeg.orientationToString(orientationValue);
```



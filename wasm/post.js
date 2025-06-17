
// This is part of the QB3 decoder WASM module

// Get the information about the QB3 raster, returns a JSON object
Module.getInfo = function (data) {
    // Use a small array, to avoid the full size
    if (data.length > 1000)
        data = data.slice(0, 1000);
    let dataPtr = Module._malloc(data.length);
    Module.writeArrayToMemory(data, dataPtr);
    let cresult = Module.ccall('GetInfo', // C function name
        'number', // return type
        ['number', 'number'], // argument types
        [dataPtr, data.length]); // arguments
    Module._free(dataPtr);
    // This is actually a json string
    if (cresult == 0)
        return null;
    let response = this.UTF8ToString(cresult);
    Module._free(cresult);
    return JSON.parse(response);
}

// Decode the QB3 raster, returns a JSON object that includes the decoded data
// as a typed array, key "data".
// The rest of the object includes metadata about the image
//
// The expectedImage parameter is used to confirm that the data matches the expected 
// format, which includes:
// - xsize: width of the image
// - ysize: height of the image
// - nbands: number of bands in the image
// - dtype: data type of the image <u>int[8|16|32|64]
Module.decode = function (data, expectedImage) {
    // Check the expected format matches the data
    let image = Module.getInfo(data);
    if (!image) {
        console.error("Failed to decode QB3 raster");
        return null;
    }

    // Check the expected format matches the data
    if (image.xsize !== expectedImage.xsize ||
        image.ysize !== expectedImage.ysize ||
        image.nbands !== expectedImage.nbands ||
        image.dtype !== expectedImage.dtype) {
        console.error("Expected format does not match the data");
        console.error("Expected: ", expectedImage);
        console.error("Got: ", image);
        return null;
    }

    // The decoder is fine, but the code that follows is not
    if (image.dtype != "uint16") {
        console.error("Unsupported data type: " + image.type);
        return null;
    }

    // Copy the data to wasm
    let rawqb3 = Module._malloc(data.length);
    Module.writeArrayToMemory(data, rawqb3);

    // Image size in bytes
    let imageSize = image.xsize * image.ysize * image.nbands * 2;
    // Allocate the output buffer
    let outPtr = Module._malloc(imageSize);
    // Allocate a buffer for the message
    let message = Module._malloc(1024);
    let decoded = ccall('decode',
        'number', // return type
        ['number', 'number', 'number', 'number'], // argument types
        [rawqb3, data.length, outPtr, message] // arguments
    )
    // Free the raw data buffer
    Module._free(rawqb3);
    if (decoded == 0) {
        Module._free(outPtr);
        console.error("Decoding failed: " + Module.UTF8ToString(message));
        Module._free(message);
        return null; // Decoding failed
    }
    Module._free(message);

    // Copy from a typed view, then release it
    image.data = new Uint16Array(new Uint16Array(Module.HEAPU8.buffer,
        outPtr, image.xsize * image.ysize * image.nbands));
    Module._free(outPtr);
    return image;
}

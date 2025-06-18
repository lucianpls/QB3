
// This is the JS friendly API of the QB3 decoder WASM module

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

    const qb3types = {
        "uint8": { 'size': 1, 'constructor': Uint8Array },
        "uint16": { 'size': 2, 'constructor': Uint16Array },
        "uint32": { 'size': 4, 'constructor': Uint32Array },
        "uint64": { 'size': 8, 'constructor': BigUint64Array },
        "int8": { 'size': 1, 'constructor': Int8Array },
        "int16": { 'size': 2, 'constructor': Int16Array },
        "int32": { 'size': 4, 'constructor': Int32Array },
        "int64": { 'size': 8, 'constructor': BigInt64Array }
    }

    try {
        var type = qb3types[image.dtype];
    } 
    catch (e) {
        console.error("Unsupported data type: " + image.dtype);
        return null;
    }

    // Copy the data to wasm
    let rawqb3 = Module._malloc(data.length);
    Module.writeArrayToMemory(data, rawqb3);

    // Image size in bytes
    let imageSize = image.xsize * image.ysize * image.nbands * type.size;
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

    image.data = new type.constructor(Module.HEAPU8.slice(outPtr, outPtr + imageSize).buffer);
    Module._free(outPtr);
    return image;
};
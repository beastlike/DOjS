/********
 * File
 */
/**
 * Open a file, for file modes see {@link FILE}. Files can only either be read or written, never both.Writing to a closed file throws an exception.
 * @class
 * @param {string} filename the name of the file.
 * @param {string} mode READ, WRITE or APPEND.
 */
function File(filename, mode) { }
/**
 * Read a single byte from file and return it as number.
 * @returns {number} the byte as a number or null for EOF.
 */
File.prototype.ReadByte = function () { };
/**
 * Write a single byte to a file.
 */
File.prototype.WriteByte = function (ch) { };
/**
 * Read a line of text from file. The maximum line length is 4096 byte.
 * @returns {string} the next line or null for EOF.
 */
File.prototype.ReadLine = function () { };
/**
 * Write a NEWLINE terminated string to a file.
 * @param {string} txt the string to write.
 */
File.prototype.WriteLine = function (txt) { };
/**
 * Write a string to a file.
 * @param {string} txt the string to write.
 */
File.prototype.WriteString = function (txt) { };
/**
 * Close the file.
 */
File.prototype.Close = function () { };

/********
 * Bitmap
 */
/**
 * Load a BMP, TGA or PCX image.
 * @class
 * @param {string} filename name of the BMP or PNG file.
 */
function Bitmap(filename) { };
/**
 * Name of the file.
 */
Bitmap.filename = null;
/**
 * Width in pixels
 */
Bitmap.width = null;
/**
 * Height in pixels
 */
Bitmap.height = null;

/**
 * Draw the image to the canvas at given coordinates.
 * @param {number} x position to draw to.
 * @param {number} y position to draw to.
 */
Bitmap.prototype.Draw = function (x, y) { };

/**
 * Draw the image to the canvas at given coordinates using the alpha channel transparency. Only works for 32bit TGA with alpha channel information.
 * @param {number} x position to draw to.
 * @param {number} y position to draw to.
 */
Bitmap.prototype.DrawTrans = function (x, y) { };

/**
 * Get the color of a pixel of this image.
 * @param {number} x position.
 * @param {number} y position.
 * @returns {number} the color of the pixel.
 */
Bitmap.prototype.GetPixel = function (x, y) { };

/********
 * Font
 */
/**
 * Load a '.FNT' file for GRX.
 * @class
 * @param {*} filename name of the font file.
 */
function Font(filename) { }
/**
 * Name of the FNT file.
 */
Font.filename = null;
/**
 * Font height
 */
Font.height = null;
/**
 * Draw a left aligned string to the canvas.
 * @param {number} x x position
 * @param {number} y y position.
 * @param {Color} foreground foreground color.
 * @param {Color} background background color.
 */
Font.prototype.DrawStringLeft = function (x, y, text, foreground, background) { };
/**
 * Draw a center aligned string to the canvas.
 * @param {number} x x position
 * @param {number} y y position.
 * @param {Color} foreground foreground color.
 * @param {Color} background background color.
 */
Font.prototype.DrawStringCenter = function (x, y, text, foreground, background) { };
/**
 * Draw a right aligned string to the canvas.
 * @param {number} x x position
 * @param {number} y y position.
 * @param {Color} foreground foreground color.
 * @param {Color} background background color.
 */
Font.prototype.DrawStringRight = function (x, y, text, foreground, background) { };
/**
 * Calculate string width for this font.
 * @param {string} the string to check.
 * @returns {number} the width in pixels.
 */
Font.prototype.StringWidth = function (text) { };
/**
 * Calculate string height for this font.
 * @param {string} the string to check.
 * @returns {number} the height in pixels.
 */
Font.prototype.StringHeight = function (text) { };


/********
 * Sample/Module
 */
/**
 * Load a WAV-file.
 * @class
 * @param {string} filename 
 */
function Sample(filename) { }
/**
 * Name of the WAV.
 */
Sample.filename = null;
/**
 * Sound length.
 */
Sample.length = null;
/**
 * Sound frequency.
 */
Sample.frequency = null;
/**
 * Play the WAV.
 * @param {number} volume between 0-255.
 * @param {number} panning between (left) 0-255 (right).
 * @param {boolean} loop true := sample will loop, false := sample will only be played once.
 */
Sample.prototype.Play = function (volume, panning, loop) { };
/**
 * Stop playing.
 */
Sample.prototype.Stop = function () { };

/********
 * MIDI
 */
/**
 * Load a midi file.
 * @class
 * @param {string} filename the name of the MIDI file.
 */
function Midi(filename) { }
/**
 * Play the midi file.
 */
Midi.prototype.Play = function () { };


/********
 * ZBuffer
 */
/**
 * Create a ZBuffer. A ZBuffer must be set as current before using it.
 * @class
 * @param {Bitmap} bitmap the bitmap to draw to or null for the screen.
 */
function ZBuffer(bitmap) { }
/**
 * Clear ZBuffer with given value.
 * @param {number} z value to use for clearing.
 */
ZBuffer.prototype.Clear = function (z) { };
/**
 * Set this ZBuffer as current.
 */
ZBuffer.prototype.Set = function () { };

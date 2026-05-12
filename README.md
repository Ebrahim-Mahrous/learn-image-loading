# Learn: Image Loading

## Status:
- Currently supports only PNG files.

## Modules:
- png.h: A library for parsing png files.
- image.h: A high level library for loading images. Since currently PNG files are the only type supported, it includes functionality to load them into the simple `Image` structure. It also includes functionality to store PNG files.

## Example:
There is an example in `example.c`. A debugger is recommended to view the raw image data in memory. Aternatively, you can use a graphics API to display the images on the screen.

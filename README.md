# Daily Desktop Display Calendar for Inkplate
A daily desktop firmware for the Inkplate 6 color https://soldered.com/product/inkplate-6color-color-e-paper-board-copy/
This will display a new image every day. If you make the images with the month and day on them, it becomes a daily desktop calendar. 
It will wake up at roughly 3am everyday, get the time/date from the NTP server, display the appropriate image based on image name, then go back to sleep with the appropriate delay for the next day.

How to use:

1. Setup Inkplate 6 in arduino editor
2. Install ArduinoJson library
3. Flash to Inkplate
4. Copy the D3Config.json to the root of the sdcard and edit with your wifi creds, timezone seconds from GMT and NTP server
5. make a directory on the sdcard called "images" where you put your images of the correct size and type (supports jpg, png, and bmp). Format of filenames is YYYY-M-D.jpg
6. Put SD card in Inkplate 
7. Turn Inkplate on

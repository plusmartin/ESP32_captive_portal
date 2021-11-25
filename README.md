# ESP32_captive_portal

This is an example of a simple captive portal setup for ESP32 running the Arduino frame, programmedd in Platformio.

It creates a webserver for any device to connect and provide Wifi credentials, which are then stored.
If wifi changes the ESP32 will attempt to connect to the known wifi while at the same time launch an access point wifi (captive portal) where the client can provide a new credential.

If the list of wifis does not show the desired wifi simply refresh the page and the ESP32 will perform a wifi re-scan.

Once the ESP32 has connected to a wifi it will close the AP.

It was inspired and built upon https://iotespresso.com/esp32-launch-captive-portal-only-if-wifi-connect-fails/#comment-426

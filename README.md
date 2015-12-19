# geowifi
This is a Geographic WiFi Positioning program written under the Linux. it use
the MAC addresses of WiFi in you neighborhood, if you want to use it to
positioning you location. you need to specifies two or more WiFi MAC addresses
in you around. and also, you must have internet to access the server of Google.

If you are China, you might need a VPN or Proxy to let it access the Google.

# Implementation
I use the libjansson and libcurl to process the requests and communicate with
Google Geolocation Server.  and if you want to use it in your program or products. there are some APIs for you to use:

        struct geowifi *geowifi_new(const char *key);

        /* parameters:
         *   mac        - (required) The MAC address of the WiFi node.
         *                Separators must be : (colon) and hex digits must use
         *                uppercase.
         *   strength   - The current signal strength measured in dBm.
         *   channel    - The channel over which the client is communicating
         *                with the access point.
         *   age        - The number of milliseconds since this access point was
         *                detected.
         *   noiseratio - The current signal to noise ratio measured in dB.
         */
        int geowifi_access_points_add(struct geowifi *gw, const char *mac,
                                      int strength, int channel,
                                      int age, int noiseratio);

        /* Performance a actually lookup for the location. */
        int geowifi_lookup_location(struct geowifi *gw);

        void geowifi_delete(struct geowifi *gw);

The implementation was according the documents of Google Geolocation API. you
can find it in this address: https://developers.google.com/maps/documentation/geolocation/intro


# Example
/*
 * This is the test result from my house. (Accuracy: very well)
 *
 * $ ./geowifi_main "96:74:2A:A9:40:EA" "28:2C:B2:AB:7B:4A"
 * Geolocation looup finished:
 *    latitude:     39.457333
 *    longitude:    75.973906
 *    accuracy:     73.000000
 */

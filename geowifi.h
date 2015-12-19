#ifndef _GEOWIFI_H
#define _GEOWIFI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <curl/curl.h>

/* Request format of Google Geolocation. */
#define GW_REQUEST	"https://www.googleapis.com/geolocation/v1/"\
			"geolocate?key=%s"

/* This is the key register by myself for personal use. it will be used when
 * you want to use Goole Geolocation API.
 * Note: if you want to use this program or API in you product. you need to
 * have your own key on the Google.
 */
#define GW_KEY		"AIzaSyBlBhYTM8TO7oRFvls3CdXerIhMTGMNiEw"


#define GW_URL_BUFSZ	1024
#define GW_DATA_BUFSZ	4096
#define GW_JSON_BUFSZ	4096
#define GW_ERR_BUFSZ	1024

/* 
 * This structure will store the arguments which named "wifiAccessPoints".
 */
struct wifi{
	char	wf_mac[18];	/* string of MAC address */
	int	wf_strength;	/* signal strength	 */
	int	wf_age;		/* ms since WiFi was detected */
	int	wf_channel;	/* channel of WiFI	 */
	int	wf_noiseratio;	/* signal to noise ratio */

	struct wifi	*wf_next;	/* pointer to next "wifiAccessPoints" */
};

struct geowifi {
	char		*gw_url;	/* receive the request	*/
	char		*gw_data;	/* store response	*/
	char		*gw_json;	/* Json forms request	*/
	char		*gw_error;	/* store the error message if have */
	struct wifi	*gw_wifi;	/* this is a pointer array */
	
	size_t		gw_data_len;	/* length of data	*/
	size_t		gw_json_len;	/* length of Json	*/
	size_t		gw_json_pos;	/* used by read_callback*/

	struct {
		double	gwl_lat;	/* latitude  */
		double	gwl_lng;	/* longitude */
		double	gwl_acr;	/* accuracy  */
	} gw_loc;
};


struct geowifi *geowifi_new(const char *key);
int geowifi_access_points_add(struct geowifi *gw, const char *mac,
                              int strength, int channel,
                              int age, int noiseratio);
int geowifi_lookup_location(struct geowifi *gw);
void geowifi_delete(struct geowifi *gw);

/* following macros will be make more sense than using pointer reference. */
#define geowifi_get_latitude(gw)	((gw)->gw_loc.gwl_lat)
#define geowifi_get_longitude(gw)	((gw)->gw_loc.gwl_lng)
#define geowifi_get_accuracy(gw)	((gw)->gw_loc.gwl_acr)

#endif

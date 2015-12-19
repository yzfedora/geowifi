/* This is a Geographic WiFi Positioning program written under the Linux.
 *   		Copyright (C) 2015  Yang Zhang <yzfedora@gmail.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************/
#include "geowifi.h"


/*
 * We used the member gw_json in the geowifi structure to store
 * the Json forms request.
 */
static size_t geowifi_curl_read_callback(char *buffer, size_t size,
					 size_t nitems, void *instream)
{
	struct geowifi *gw = instream;
	size_t nwrt = size * nitems;

	if (gw->gw_json_len > 0) {
		/*
		 * if no enough space store data to the buffer, then write most
		 * size * nitems bytes, and the remain data will be store to
		 * buffer in the next time.
		 */
		nwrt = gw->gw_json_len <= nwrt ? gw->gw_json_len : nwrt;
		memcpy(buffer, gw->gw_json + gw->gw_json_pos, nwrt);
		gw->gw_json_pos += nwrt;
		gw->gw_json_len -= nwrt;
		return nwrt;
	}
	
	return 0;
}

/*
 * libcurl callback function, we store the data to the member gw_data in the
 * geowifi structure.
 */
static size_t geowifi_curl_write_callback(char *ptr, size_t size, size_t nmemb,
		                          void *userdata)
{
	struct geowifi *gw = userdata;
	size_t nread = size * nmemb;

	/* Prevent the buffer overflow. */
	if ((gw->gw_data_len + nread) >= GW_DATA_BUFSZ) {
		fprintf(stderr, "no enough space to store the data received "
				"from libcurl.\n");
		return 0;
	}

	memcpy(gw->gw_data + gw->gw_data_len, ptr, nread);
	gw->gw_data_len += nread;
	gw->gw_data[gw->gw_data_len] = 0;

	return nread;
}

/*
 * Store the Json formatted request into gw_json. later will be read_callback
 * function read into curl object.
 * Note: the Google Geolocation API specifies the number of"wifiAccessPoints"
 * must TWO or MORE.
 */
static void geowifi_json_set_request(struct geowifi *gw)
{
	json_t *root = json_object();
	json_t *wifi = json_array();
	struct wifi *wf = gw->gw_wifi;

	/*
	 * Traversal the linked-list gw_wifi. add those node as the elements
	 * in the "wifiAccessPoints" array. This is according to the Google
	 * Geolocation API.
	 */
	while (wf) {
		json_t *j = json_object();
		
		json_object_set_new(j, "macAddress", json_string(wf->wf_mac));
		json_object_set_new(j, "signalStrength",
				    json_integer(wf->wf_strength));

		json_object_set_new(j, "age", json_integer(wf->wf_age));
		json_object_set_new(j, "channel", json_integer(wf->wf_channel));
		
		json_object_set_new(j, "signalToNoiseRatio",
				    json_integer(wf->wf_noiseratio));
		
		json_array_append(wifi, j);
		json_decref(j);

		wf = wf->wf_next;
	}
	

	json_object_set_new(root, "wifiAccessPoints", wifi);

	gw->gw_json = json_dumps(root, 0);
	gw->gw_json_len = strlen(gw->gw_json);

	/* Decrease the reference counter for json free resource later. */
	json_decref(wifi);
	json_decref(root);
}

/*
 * Set all necessary options for libcurl. including URL, HTTP Headers,
 * POST way to send and callback functions both read and write.
 */
static void geowifi_curl_setopts(CURL *curl, struct curl_slist **headers,
				 struct geowifi *gw)
{
	curl_easy_setopt(curl, CURLOPT_URL, gw->gw_url);

	/* Set Content-Type, and charsets. */
	*headers = curl_slist_append(*headers,
				    "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *headers);

	/* Set use POST way to send data. */
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, gw->gw_json_len);
	
	/* Set callback functions for read and write */
	curl_easy_setopt(curl, CURLOPT_READFUNCTION,
			 geowifi_curl_read_callback);
	curl_easy_setopt(curl, CURLOPT_READDATA, gw);
	
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			 geowifi_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, gw);
}


/*
 * Append wifi structure to the linked-list gw_wifi.
 */
static void geowifi_access_points_add_wifi(struct geowifi *gw, struct wifi *wf)
{
	struct wifi **save, *tmp = gw->gw_wifi;

	/* Add the wifi structure to the end of linked-list. */
	save = &gw->gw_wifi;
	while (tmp) {
		save = &tmp->wf_next;
		tmp = wf->wf_next;
	}
	
	*save = wf;
}

/*
 * Parsing the errors responded from server. and store the reason message
 * in to field gw_error.
 */
static void geowifi_json_get_error(struct geowifi *gw)
{
	json_error_t err;
	json_t *root = json_loads(gw->gw_data, 0, &err);
	json_t *error;

	if (!root) {
		snprintf(gw->gw_error, GW_ERR_BUFSZ,
			 "json_load error: line %d, %s\n",
			 err.line, err.text);
		return;
	}

	if ((error = json_object_get(root, "error")) &&
	    json_is_object(error)) {
		json_t *code = json_object_get(error, "code");
		json_t *msg = json_object_get(error, "message");

		if (code && msg)
			snprintf(gw->gw_error, GW_ERR_BUFSZ,
				 "error occurs, code=%ld, reason: %s\n",
				 (long)json_integer_value(code),
				 json_string_value(msg));
	}

}

/*
 * Parsing the data we received which stored as form of Json. extract the
 * latitude, longitude and accuracy. if all goes well, return 0. otherwise
 * -1 will be returned.
 */
static int geowifi_json_get_location(struct geowifi *gw)
{
	json_error_t err;
	json_t *loc, *acr;
	json_t *lat, *lng;
	json_t *root = json_loads(gw->gw_data, 0, &err);

	if (!root) {
		snprintf(gw->gw_error, GW_ERR_BUFSZ,
			 "json_load error: line %d, %s\n",
			 err.line, err.text);
		return -1;
	}

	if ((loc = json_object_get(root, "location")) &&
	    (lat = json_object_get(loc, "lat")) &&
	    (lng = json_object_get(loc, "lng")) &&
	    (acr = json_object_get(root, "accuracy"))) {
		/* Store the location and accuracy to the field gw_loc. */
		gw->gw_loc.gwl_lat = json_real_value(lat);
		gw->gw_loc.gwl_lng = json_real_value(lng);
		gw->gw_loc.gwl_acr = json_real_value(acr);

		return 0;
	} else {
		snprintf(gw->gw_error, GW_ERR_BUFSZ,
			 "error occurs when parsing the location data.\n");
		return -1;
	}
}

/*
 * If add the access points successfully, 0 will be returned. if memory
 * allocated failure, the -1 will be returned.
 */
int geowifi_access_points_add(struct geowifi *gw, const char *mac,
			      int strength, int age,
			      int channel, int noiseratio)
{
	struct wifi *wf = calloc(1, sizeof(*wf));

	if (!wf)
		return -1;

	strncpy(wf->wf_mac, mac, sizeof(wf->wf_mac));
	wf->wf_strength = strength;
	wf->wf_channel = channel;
	wf->wf_age = age;
	wf->wf_noiseratio = noiseratio;

	geowifi_access_points_add_wifi(gw, wf);
	
	return 0;
}

/*
 * Use Libcurl send Json requset to the Google Geolocation Server.
 * return 0 on success, otherwise, -1 will be returned, and the error
 * message will be store in to field gw_error.
 */
int geowifi_lookup_location(struct geowifi *gw)
{
	int ret = -1;
	int err;
	long int http_code;
	struct curl_slist *headers = NULL;
	CURL *curl;


	curl_global_init(CURL_GLOBAL_ALL);
	if (!(curl = curl_easy_init()))
		goto out;

	/* 
	 * MUST CALL geowifi_json_set_request() first, it will convert the
	 * request informations in the member of gw_wifi linked-list to Json
	 * string, and set the gw_json_[pos|len|] appropriately.
	 */
	geowifi_json_set_request(gw);

	/*
	 * Call this function to set the URL, HTTPHEADER, POST, POSTFIELDSIZE
	 * and the CALLBACK-FUNCTIONS.
	 */
	geowifi_curl_setopts(curl, &headers, gw);

	if ((err = curl_easy_perform(curl))) {
		snprintf(gw->gw_error, GW_ERR_BUFSZ,
			 "curl_easy_perform: %s\n", curl_easy_strerror(err));
		goto out;
	}

	/* 
	 * If any HTTP/HTTPS error was happened, a description about error
	 * will be store in the gw_error.
	 */
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code != 200) {
		geowifi_json_get_error(gw);
		goto out;
	}


	/* Finally, we got the location according two or more MAC Addresses. */
	if (geowifi_json_get_location(gw) == -1)
		goto out;

	ret = 0;		/* when reach here, means success */
out:

	if (headers)
		curl_slist_free_all(headers);
	if (curl)
		curl_easy_cleanup(curl);
	
	curl_global_cleanup();
	return ret;
}

void geowifi_delete(struct geowifi *gw)
{
	if (NULL == gw)
		return;

	if (gw->gw_url)
		free(gw->gw_url);
	if (gw->gw_data)
		free(gw->gw_data);
	if (gw->gw_error)
		free(gw->gw_error);

	struct wifi *tmp, *wf = gw->gw_wifi;

	while (wf) {
		tmp = wf;
		wf = wf->wf_next;
		free(tmp);
	}

	free(gw);
}


/*
 * Create an geowifi object which store the infos we need.
 */
struct geowifi *geowifi_new(const char *key)
{
	struct geowifi *gw = calloc(1, sizeof(*gw));

	if (!gw)
		goto out;

	if (!(gw->gw_url = malloc(GW_URL_BUFSZ)) || 
	    !(gw->gw_data = malloc(GW_DATA_BUFSZ)) ||
	    !(gw->gw_error = malloc(GW_ERR_BUFSZ)))
		goto out;


	/* Initial the url of we request to. */
	snprintf(gw->gw_url, GW_URL_BUFSZ, GW_REQUEST, key);


	return gw;
out:
	geowifi_delete(gw);
	return NULL;
}

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
/*
 * This is the test result from my house. (Accuracy: very well)
 *
 * $ ./geowifi_main "96:74:2A:A9:40:EA" "28:2C:B2:AB:7B:4A"
 * Geolocation looup finished:
 *    latitude:     39.457333
 *    longitude:    75.973906
 *    accuracy:     73.000000
 */
#include <unistd.h>
#include "geowifi.h"

static void usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [options] <WAP> <WAP>...\n", progname);
	fprintf(stderr, "    <WAP>  Called WiFi Access Points, which consists\n"
			"           by following five fields:\n\n"
			"           <Field>              <prefix> <is-required>"
			"           MAC Address           <first>  required\n"
			"           Signal Strength        's'     optional\n"
			"           Age                    'a'     optional\n"
			"           Channel                'c'     optional\n"
			"           Signal to Noise Ratio  'n'     optional\n"
			"    -h     for this help\n");

	fprintf(stderr, "Example:\n"
			"    $ %s \"AA:BB:CC:00:11:22 s-65 a0 c11 n40\" "
			"\"AA:BB:CC:00:11:33 s13 c1\"\n",
			progname);
}

/*
 * Parsing the string of argument WiFi Access Points(WAP).
 */
static int add_access_points_by_args(struct geowifi *gw, char *args)
{
	int ret = -1;
	int strength = 0, age = 0, channel = 0, noiseratio = 0;
	char *next, *tmp = args;

	if (!args || !*args) {
		fprintf(stderr, "Missing MAC Address.\n");
		goto out;
	}
	
	while (tmp && *tmp) {
		if ((next = strchr(tmp, ' ')))
			*next++ = 0;
		
		if (tmp == args)		/* skipping MAC address */
			goto next;

		switch (*tmp) {
		case 's':
			strength = atoi(++tmp);
			break;
		case 'a':
			age = atoi(++tmp);
			break;
		case 'c':
			channel = atoi(++tmp);
			break;
		case 'n':
			noiseratio = atoi(++tmp);
			break;
		default:
			fprintf(stderr, "Unknown prefix in the argument: %c\n",
					*tmp);
			goto out;
		}
next:
		tmp = next;
	}

	/*
	 * Add a Wifi Access Points Object to the geowifi structure.
	 */
	ret = geowifi_access_points_add(gw, args, strength, age, channel,
					noiseratio);

out:
	return ret;
}

static int parsing_args(struct geowifi *gw, int argc, char *argv[])
{
	int ret = -1;
	int opt;

	/* We intend to use getopt() for extension our options later. */
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
		case 'h':
		default:
			usage(argv[0]);
			goto out;
		}
	}

	/*
	 * Note: there at least require two WAP argument for Google Geolocation
	 * to use. if less, it will return 404.
	 */
	if (argc - optind < 2) {
		fprintf(stderr, "There requires two WAP arguments to locate\n");
		goto out;
	}

	while (optind < argc) {
		/* if any error occur, terminate the process. and go out. */
		if ((ret = add_access_points_by_args(gw, argv[optind++])))
			break;
	}

out:
	return ret;
}

int main(int argc, char *argv[])
{
	struct geowifi *gw = geowifi_new(GW_KEY);

	if (!gw) {
		fprintf(stderr, "unable to create geowifi object.\n");
		return 1;
	}

	if (parsing_args(gw, argc, argv) == -1) {
		fprintf(stderr, "error occurs when parsing the arguments.\n");
		return 1;
	}

	/* 
	 * Starting the request Google Geolocation Server to lookup our
	 * location by WiFi MAC addresses.
	 */
	if (geowifi_lookup_location(gw) == -1) {
		fprintf(stderr, "%s", gw->gw_error);
		return 1;
	}

	printf("Geolocation lookup finished:\n"
	       "    latitude:     %f\n"
	       "    longitude:    %f\n"
	       "    accuracy:     %f\n\n", geowifi_get_latitude(gw),
	       geowifi_get_longitude(gw), geowifi_get_accuracy(gw));
	
	geowifi_delete(gw);
	return 0;
}

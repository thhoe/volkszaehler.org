/**
 * Implementation of local interface via libmicrohttpd
 *
 * @package vzlogger
 * @copyright Copyright (c) 2011, The volkszaehler.org project
 * @license http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author Steffen Vogel <info@steffenvogel.de>
 */
/*
 * This file is part of volkzaehler.org
 *
 * volkzaehler.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * volkzaehler.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with volkszaehler.org. If not, see <http://www.gnu.org/licenses/>.
 */

#include <json/json.h>
#include <string.h>
 
#include "main.h"
#include "local.h"

extern channel_t *chans;
extern options_t opts;

int handle_request(void *cls, struct MHD_Connection *connection, const char *url, const char *method,
			const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {
	const char * json_str;
	const char * uuid = url + 1;
	int ret;
	int num_chans = *(int *) cls;
	
	print(2, "Local request reveived: %s %s %s", NULL, version, method, url);
	
	struct MHD_Response *response;
	
	struct json_object * json_obj = json_object_new_object();
	struct json_object * json_data = json_object_new_object();

	for (int i = 0; i < num_chans; i++) {
		channel_t *ch = &chans[i];
		reading_t rd;
		
		if (strcmp(ch->uuid, uuid) == 0) {
			pthread_mutex_lock(&ch->mutex);
			pthread_cond_wait(&ch->condition, &ch->mutex); /* wait for new data comet-like blocking of HTTP response */
			pthread_mutex_unlock(&ch->mutex);
		
			struct json_object * json_tuples = json_object_new_array();
		
			int j = ch->queue.write_p;
			do {
				pthread_mutex_lock(&ch->mutex);
				rd = ch->queue.buf[j];
				pthread_mutex_unlock(&ch->mutex);
		
				if (rd.value != 0) { /* skip invalid / empty readings */
					struct json_object * json_tuple = json_object_new_array();
			
					int timestamp = rd.tv.tv_sec * 1000 + rd.tv.tv_usec / 1000;
		
					json_object_array_add(json_tuple, json_object_new_int(timestamp));
					json_object_array_add(json_tuple, json_object_new_double(rd.value));
					json_object_array_add(json_tuple, json_object_new_int(1)); /* just for middleware compability */
		
					json_object_array_add(json_tuples, json_tuple);
				}
			
				j++;
				j %= ch->queue.size;
			} while (j != ch->queue.read_p);
				
			json_object_object_add(json_data, "uuid", json_object_new_string(ch->uuid));
			json_object_object_add(json_data, "interval", json_object_new_int(ch->interval));
			json_object_object_add(json_data, "tuples", json_tuples);
		}
	}
	
	json_object_object_add(json_obj, "version", json_object_new_string(VZ_VERSION));
	json_object_object_add(json_obj, "data", json_data);
	json_str = json_object_to_json_string(json_obj);
	
	response = MHD_create_response_from_data(strlen(json_str), (void *) json_str, FALSE, TRUE);
	
	MHD_add_response_header(response, "Content-type", "application/json");
	
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	
	MHD_destroy_response (response);

	return ret;
}

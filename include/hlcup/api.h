#pragma once
#include <inttypes.h>
#include <sys/uio.h>

#include "objects.h"

void api_init();
void api_show_cnt();

int users_new(char **body, const char *e);
user_t* users_get(uint32_t id);
int users_update(uint32_t id, char *body, const char *e);

int locations_new(char **body, const char *e);
location_t* locations_get(uint32_t id);
int locations_update(uint32_t id, char *body, const char *e);

int visits_new(char **body, const char *e);
visit_t* visits_get(uint32_t id);
int visits_update(uint32_t id, char *body, const char *e);

int locations_avg(uint32_t locationid, uint32_t from_date, uint32_t to_date, uint32_t from_age, uint32_t to_age, char gender, char *result);
int users_visits(uint32_t userid, uint32_t from_date, uint32_t to_date, const char *country, uint32_t to_distance, char **out);

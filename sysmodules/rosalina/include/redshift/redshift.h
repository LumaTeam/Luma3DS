/* redshift.h -- Main program header
   This file is part of Redshift.

   Redshift is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Redshift is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Redshift.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2013-2017  Jon Lund Steffensen <jonlst@gmail.com>
*/

#ifndef REDSHIFT_REDSHIFT_H
#define REDSHIFT_REDSHIFT_H

#include <stdio.h>
#include <stdlib.h>

/* The color temperature when no adjustment is applied. */
#define NEUTRAL_TEMP  6500


/* Location */
typedef struct {
	float lat;
	float lon;
} location_t;

/* Periods of day. */
typedef enum {
	PERIOD_NONE = 0,
	PERIOD_DAYTIME,
	PERIOD_NIGHT,
	PERIOD_TRANSITION
} period_t;

/* Color setting */
typedef struct {
	int temperature;
	float gamma[3];
	float brightness;
} color_setting_t;

/* Program modes. */
typedef enum {
	PROGRAM_MODE_CONTINUAL,
	PROGRAM_MODE_ONE_SHOT,
	PROGRAM_MODE_PRINT,
	PROGRAM_MODE_RESET,
	PROGRAM_MODE_MANUAL
} program_mode_t;

/* Time range.
   Fields are offsets from midnight in seconds. */
typedef struct {
	int start;
	int end;
} time_range_t;

/* Transition scheme.
   The solar elevations at which the transition begins/ends,
   and the association color settings. */
typedef struct {
	double high;
	double low;
	int use_time; /* When enabled, ignore elevation and use time ranges. */
	time_range_t dawn;
	time_range_t dusk;
	color_setting_t day;
	color_setting_t night;
} transition_scheme_t;


/* Gamma adjustment method */
typedef struct gamma_state gamma_state_t;

typedef int gamma_method_init_func(gamma_state_t **state);
typedef int gamma_method_start_func(gamma_state_t *state);
typedef void gamma_method_free_func(gamma_state_t *state);
typedef void gamma_method_print_help_func(FILE *f);
typedef int gamma_method_set_option_func(gamma_state_t *state, const char *key,
					 const char *value);
typedef void gamma_method_restore_func(gamma_state_t *state);
typedef int gamma_method_set_temperature_func(
	gamma_state_t *state, const color_setting_t *setting, int preserve);

typedef struct {
	char *name;

	/* If true, this method will be tried if none is explicitly chosen. */
	int autostart;

	/* Initialize state. Options can be set between init and start. */
	gamma_method_init_func *init;
	/* Allocate storage and make connections that depend on options. */
	gamma_method_start_func *start;
	/* Free all allocated storage and close connections. */
	gamma_method_free_func *free;

	/* Print help on options for this adjustment method. */
	gamma_method_print_help_func *print_help;
	/* Set an option key, value-pair */
	gamma_method_set_option_func *set_option;

	/* Restore the adjustment to the state before start was called. */
	gamma_method_restore_func *restore;
	/* Set a specific color temperature. */
	gamma_method_set_temperature_func *set_temperature;
} gamma_method_t;


/* Location provider */
typedef struct location_state location_state_t;

typedef int location_provider_init_func(location_state_t **state);
typedef int location_provider_start_func(location_state_t *state);
typedef void location_provider_free_func(location_state_t *state);
typedef void location_provider_print_help_func(FILE *f);
typedef int location_provider_set_option_func(
	location_state_t *state, const char *key, const char *value);
typedef int location_provider_get_fd_func(location_state_t *state);
typedef int location_provider_handle_func(
	location_state_t *state, location_t *location, int *available);

typedef struct {
	char *name;

	/* Initialize state. Options can be set between init and start. */
	location_provider_init_func *init;
	/* Allocate storage and make connections that depend on options. */
	location_provider_start_func *start;
	/* Free all allocated storage and close connections. */
	location_provider_free_func *free;

	/* Print help on options for this location provider. */
	location_provider_print_help_func *print_help;
	/* Set an option key, value-pair. */
	location_provider_set_option_func *set_option;

	/* Listen and handle location updates. */
	location_provider_get_fd_func *get_fd;
	location_provider_handle_func *handle;
} location_provider_t;


#endif /* ! REDSHIFT_REDSHIFT_H */
/* Copyright (c) 2014 Scott Kuhl. All rights reserved.
 * License: This code is licensed under a 3-clause BSD license. See
 * the file named "LICENSE" for a full copy of the license.
 */

/** @file
 * @author Scott Kuhl
 */
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <string>

#ifndef MISSING_VRPN
#include <vrpn_Tracker.h>
#include <quat.h>
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "kuhl-util.h"
#include "vecmat.h"

#ifndef MISSING_VRPN

/** A mapping of object\@tracker strings to vrpn_Tracker_Remote objects
 * so we can quickly find the appropriate object given an
 * object\@tracker string. */
std::map<std::string, vrpn_Tracker_Remote*> nameToTracker;
/** A mapping of object\@tracker strings to the data that the callback
 * functions store the data into. */
std::map<std::string, vrpn_TRACKERCB> nameToCallbackData;

/** A callback function that will get called whenever the tracked
 * point moves. */
static void VRPN_CALLBACK handle_tracker(void *name, const vrpn_TRACKERCB t)
{
	// Store the data in our map so that someone can use it later.
	std::string s = (char*)name;
	nameToCallbackData[s] = t;
}

#endif

extern "C" {
/** Uses the VRPN library to get the position and orientation of a
 * tracked object.
 *
 * @param object The name of the object being tracked.
 *
 * @param hostname The IP address or hostname of the VRPN server or
 * tracking system computer. If hostname is set to NULL, the IP
 * address of the Vicon tracker in the IVS lab is used.
 *
 * @param pos An array to be filled in with the position information
 * for the tracked object. If we are unable to track the object, a
 * message may be printed and pos will be set to a fixed value.
 *
 * @param orient An array to be filled in with the orientation matrix
 * for the tracked object. The orientation matrix is in row-major
 * order can be used with OpenGL. If the tracking system is moving an
 * object around on the screen, this matrix can be used directly. If
 * the tracking system is moving the OpenGL camera, this matrix may
 * need to be inverted. If we are unable to track the object, a
 * message may be printed and orient will be set to the identity
 * matrix.
 *
 * @return 1 if we returned data from the tracker. 0 if there was
 * problems connecting to the tracker.
 */
int vrpn_get(const char *object, const char *hostname, float pos[3], float orient[16])
{
	/* Set to default values */
	vec3f_set(pos, 10000,10000,10000);
	mat4f_identity(orient);
#ifdef MISSING_VRPN
	printf("You are missing VRPN support.\n");
	return 0;
#else
	/* Construct an object@hostname string. */
	std::string hostnamecpp;
	std::string objectcpp;
	if(hostname == NULL)
	{
		/* Try reading VRPN server information from ~/.vrpn-server

		   This file should contain a single line that says something like:
		   tcp://VRPN.SERVER.IP.ADDR
		 */
		const char *homedir = getenv("HOME");
		char path[1024];
		snprintf(path, 1024, "%s/.vrpn-server", homedir);
		FILE *f = fopen(path, "r");
		if(f == NULL)
		{
			printf("%s: Can't open file %s to get VRPN server information.\n", __func__, path);
			exit(EXIT_FAILURE);
		}
		char vrpnString[1024];
		if(fscanf(f, "%1023s", vrpnString) != 1)
		{
			printf("%s: Can't read %s to get VRPN server information.\n", __func__, path);
			exit(EXIT_FAILURE);
		}
		fclose(f);
		// printf("%s: Found in %s: %s\n", __func__, path, vrpnString);
		hostnamecpp = vrpnString;
	}
	else
		hostnamecpp = hostname;

	objectcpp = object;
	std::string fullname = objectcpp + "@" + hostnamecpp;

	/* Check if we have a tracker object for that string in our map. */
	if(nameToTracker.count(fullname))
	{
		/* If we already have a tracker object, ask it to run the main
		 * loop (and therefore call our handle_tracker() function if
		 * there is new data). */
		nameToTracker[fullname]->mainloop();

		/* If our callback has been called, get the callback object
		 * and get the data out of it. */
		if(nameToCallbackData.count(fullname))
		{
			vrpn_TRACKERCB t = nameToCallbackData[fullname];
			float pos4[4];
			for(int i=0; i<3; i++)
				pos4[i] = t.pos[i];
			pos4[3]=1;

			double orientd[16];
			// Convert quaternion into orientation matrix.
			q_to_ogl_matrix(orientd, t.quat);
			for(int i=0; i<16; i++)
				orient[i] = (float) orientd[i];

			/* IMPORTANT NOTE: The Vicon tracking system is normally
			 * calibrated so that:
			 * X = points to the right (while facing screen)
			 * Y = points into the screen
			 * Z = up
			 *
			 * By default, OpenGL assumes that:
			 * X = points to the right (while facing screen)
			 * Y = up
			 * Z = points OUT of the screen (i.e., -Z points into the screen)
			 *
			 * Below, we convert the Vicon position and orientation
			 * information into the OpenGL convention to make it
			 * easier to add Vicon support to an OpenGL program.
			 *
			 * If you are using this file with a different tracking
			 * system, this conversion may be wrong!
			 */
			   
			float zUpToYUp[16] = { 1,0,0,0,  // column major order!
			                       0,0,-1,0,
			                       0,1,0,0,
			                       0,0,0,1 };
			mat4f_mult_mat4f_new(orient, zUpToYUp, orient);
			mat4f_mult_vec4f_new(pos4, zUpToYUp, pos4);
			vec3f_copy(pos,pos4);
			return 1; // we successfully collected some data
		}
	}
	else
	{
		/* If this is our first time, create a tracker for the object@hostname string, register the callback handler. */
		printf("vrpn-help: Connecting to VRPN server. If this hangs, VRPN server is not running.\n");
		vrpn_Tracker_Remote *tkr = new vrpn_Tracker_Remote(fullname.c_str());
		nameToTracker[fullname] = tkr;
		tkr->register_change_handler((void*) fullname.c_str(), handle_tracker);
	}
	return 0;
#endif
}
} // extern C

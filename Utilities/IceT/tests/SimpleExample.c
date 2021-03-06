/* -*- c -*- *****************************************************************
** $Id$
**
** Copyright (C) 2003 Sandia Corporation
** Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
** license for use of this work by or on behalf of the U.S. Government.
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that this Notice and any statement
** of authorship are reproduced on all copies.
**
** This test provides a simple example of using ICE-T to perform parallel
** rendering.
*****************************************************************************/

#include <GL/ice-t.h>
#include "test-util.h"
#include "test_codes.h"
#include "glwin.h"

#ifdef __APPLE__
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#include <stdlib.h>
#include <stdio.h>

GLint rank;
GLint num_proc;

static void draw(void)
{
    static GLUquadricObj *sphere = NULL;

    if (sphere == NULL) {
        sphere = gluNewQuadric();
        gluQuadricDrawStyle(sphere, GLU_FILL);
        gluQuadricNormals(sphere, GLU_SMOOTH);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* When changing the modelview matric in the draw function, you must be
   * wary of two things.  First, make sure the modelview matrix is restored
   * to what is was when the function is called.  Remember, the draw
   * function may be called multiple times and transformations may be
   * commuted.  Also, the bounds of the drawn geometry must be correctly
   * transformed before given to ICE-T.  ICE-T has no way of knowing about
   * transformations done here.  It is an error to change the projection
   * matrix in the draw function. */
    glPushMatrix();
      glMatrixMode(GL_MODELVIEW);
      glTranslatef((float)rank, 0, 0);
      gluSphere(sphere, 0.5, 10, 30);
    glPopMatrix();
}

int SimpleExample(int argc, char * argv[])
{
    float angle;

    /* To remove warning */
    (void)argc;
    (void)argv;

  /* Normally, the first thing that you do is set up your communication and
   * then create at least one ICE-T context.  This has already been done in
   * the calling function (i.e. icetTests_mpi.c).  See the init_mpi_comm in
   * mpi_comm.h for an example.
   */

  /* If we had set up the communication layer ourselves, we could have
   * gotten these parameters directly from it.  Since we did not, this
   * provides an alternate way. */
    icetGetIntegerv(ICET_RANK, &rank);
    icetGetIntegerv(ICET_NUM_PROCESSES, &num_proc);

  /* We should be able to set any color we want, but we should do it BEFORE
   * icetDrawFrame() is called, not in the callback drawing function.
   * There may also be limitations on the background color when performing
   * color blending. */
    glClearColor(0.2f, 0.5f, 0.1f, 1.0f);

  /* Give ICE-T a function that will issue the OpenGL drawing commands. */
    icetDrawFunc(draw);

  /* Give ICE-T the bounds of the polygons that will be drawn.  Note that
   * we must take into account any transformation that happens within the
   * draw function (but ICE-T will take care of any transformation that
   * happens before icetDrawFrame). */
    icetBoundingBoxf(-0.5f+rank, 0.5f+rank, -0.5, 0.5, -0.5, 0.5);

  /* Set up the tiled display.  Normally, the display will be fixed for a
   * given installation, but since this is a demo, we give two specific
   * examples. */
    if (num_proc < 4) {
      /* Here is an example of a "1 tile" case.  This is functionally
       * identical to a traditional sort last algorithm. */
        icetResetTiles();
        icetAddTile(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    } else {
      /* Here is an example of a 4x4 tile layout.  The tiles are displayed
       * with the following ranks:
       *
       *               +---+---+
       *               | 0 | 1 |
       *               +---+---+
       *               | 2 | 3 |
       *               +---+---+
       *
       * Each tile is simply defined by grabing a viewport in an infinite
       * global display screen.  The global viewport projection is
       * automatically set to the smallest region containing all tiles.
       *
       * This example also shows tiles abutted against each other.
       * Mullions and overlaps can be implemented by simply shifting tiles
       * on top of or away from each other.
       */
        icetResetTiles();
        icetAddTile(0,           SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
        icetAddTile(SCREEN_WIDTH,SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT, 1);
        icetAddTile(0,           0,             SCREEN_WIDTH, SCREEN_HEIGHT, 2);
        icetAddTile(SCREEN_WIDTH,0,             SCREEN_WIDTH, SCREEN_HEIGHT, 3);
    }

  /* Tell ICE-T what strategy to use.  The REDUCE strategy is an all-around
   * good performer. */
    icetStrategy(ICET_STRATEGY_REDUCE);

  /* Set up the projection matrix as you normally would. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-0.75, 0.75, -0.75, 0.75, -0.75, 0.75);

  /* Other normal OpenGL setup. */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    if (rank%8 != 0) {
        GLfloat color[4];
        color[0] = (float)(rank%2);
        color[1] = (float)((rank/2)%2);
        color[2] = (float)((rank/4)%2);
        color[3] = 1.0;
        glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, color);
    }

  /* Here is an example of an animation loop. */
    for (angle = 0; angle < 360; angle += 10) {
      /* We can set up a modelview matrix here and ICE-T will factor this
       * in determining the screen projection of the geometry.  Note that
       * there is further transformation in the draw function that ICE-T
       * cannot take into account.  That transformation is handled in the
       * application by deforming the bounds before giving them to
       * ICE-T. */
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glRotatef(angle, 0.0, 1.0, 0.0);
        glScalef(1.0f/num_proc, 1.0, 1.0);
        glTranslatef(-(num_proc-1)/2.0f, 0.0, 0.0);

      /* Instead of calling draw() directly, call it indirectly through
       * icetDrawFrame().  ICE-T will automatically handle image
       * compositing. */
        icetDrawFrame();

      /* For obvious reasons, ICE-T should be run in double-buffered frame
       * mode.  After calling icetDrawFrame, the application should do a
       * synchronize (a barrier is often about as good as you can do) and
       * then a swap buffers. */
        swap_buffers();
    }

    finalize_test(TEST_PASSED);
    return TEST_PASSED;
}

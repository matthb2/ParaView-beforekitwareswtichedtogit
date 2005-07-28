/*
 * Copyright (c) 1994 Sandia Corporation. Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Governement
 * retains certain rights in this software.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.  
 * 
 *     * Neither the name of Sandia Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/*****************************************************************************
*
* exgnm - ex_get_node_map
*
* author - Sandia National Laboratories
*          Larry A. Schoof - Original
*
*          
* environment - UNIX
*
* entry conditions - 
*   input parameters:
*       int     exoid                   exodus file id
*       int     map_id                  node map id
*
* exit conditions - 
*       int*    node_map                node map
*
* revision history - 
*
*
*****************************************************************************/

#include <stdlib.h>
#include "exodusII.h"
#include "exodusII_int.h"

/*
 * reads the node map with specified ID
 */

int ex_get_node_map (int   exoid,
                     int   map_id,
                     int  *node_map)
{
   int dimid, var_id, id_ndx, iresult;
   long num_node, start[1], count[1]; 
   nclong *longs;
   char errmsg[MAX_ERR_LENGTH];

   exerrval = 0; /* clear error code */

   /* See if any nodes are stored in this file */
   if ((dimid = ncdimid (exoid, DIM_NUM_NODES)) == -1)
   {
     return (EX_NOERR);
   }

   if (ncdiminq (exoid, dimid, (char *) 0, &num_node) == -1)
   {
     exerrval = ncerr;
     sprintf(errmsg,
            "Error: failed to get number of nodes in file id %d", exoid);
     ex_err("ex_get_node_map",errmsg,exerrval);
     return (EX_FATAL);
   }

/* first check if any node maps have been defined */

   if ((dimid = ncdimid (exoid, DIM_NUM_NM))  == -1)
   {
     exerrval = ncerr;
     sprintf(errmsg,
            "Warning: no node maps defined in file id %d",
             exoid);
     ex_err("ex_get_node_map",errmsg,exerrval);
     return (EX_WARN);
   }

/* Lookup index of node map id property array */

   id_ndx = ex_id_lkup(exoid,VAR_NM_PROP(1),map_id);
   if (exerrval != 0) 
   {

      sprintf(errmsg,
              "Error: failed to locate node map id %d in %s in file id %d",
               map_id,VAR_NM_PROP(1),exoid);
      ex_err("ex_get_node_map",errmsg,exerrval);
      return (EX_FATAL);
   }

/* inquire id's of previously defined dimensions and variables */

   if ((var_id = ncvarid (exoid, VAR_NODE_MAP(id_ndx))) == -1)
   {
     exerrval = ncerr;
     sprintf(errmsg,
            "Error: failed to locate node map %d in file id %d",
             map_id,exoid);
     ex_err("ex_get_node_map",errmsg,exerrval);
     return (EX_FATAL);
   }


/* read in the node map */

/* application code has allocated an array of ints but netcdf is expecting
   a pointer to nclongs;  if ints are different sizes than nclongs,
   we must allocate an array of nclongs then convert them to ints with ltoi */

   start[0] = 0;
   count[0] = num_node;

   if (sizeof(int) == sizeof(nclong)) {
     iresult = ncvarget (exoid, var_id, start, count, node_map);
   } else {
     if (!(longs = malloc(num_node * sizeof(nclong)))) {
       exerrval = EX_MEMFAIL;
       sprintf(errmsg,
               "Error: failed to allocate memory for node map for file id %d",
               exoid);
       ex_err("ex_get_node_map",errmsg,exerrval);
       return (EX_FATAL);
     }
     iresult = ncvarget (exoid, var_id, start, count, longs);
   }
   
   if (iresult == -1)
   {
     exerrval = ncerr;
     sprintf(errmsg,
            "Error: failed to get node map in file id %d",
             exoid);
     ex_err("ex_get_node_map",errmsg,exerrval);
     return (EX_FATAL);
   }

   if (sizeof(int) != sizeof(nclong)) {
      ltoi (longs, node_map, num_node);
      free (longs);
   }

   return (EX_NOERR);

}

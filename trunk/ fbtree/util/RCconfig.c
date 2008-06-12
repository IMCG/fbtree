/*	$NetBSD: args.c,v 1.5 1997/10/19 03:17:12 lukem Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1976 Board of Trustees of the University of Illinois.
 * Copyright (c) 1985 Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 
 * netbsdsrc/usr.bin/indent/args.c
 * Modified by Xiao Meng, 2008-04-08
 *
 * It can work. But it's a big problem to free the dynamic allocted memory :-(
 * I'll try to deal with it later
 *
 */

/*
 * Argument scanning and profile reading code.  Default parameters are set
 * here as well.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RCconfig.h"      /* option variable should be defined in this file */

/* profile types */
#define	PRO_STR	    1	/* string */
#define	PRO_INT		2	/* integer */

static char *   option_source= ".rc";	/* path of runtime control file */

struct pro {
	char   *p_name;		/* name in the RC file */
	int     p_type;		/* type (int, bool, special) */
    struct{
          char  *s;
          int   i;
    }p_default;         /* the default value : I intened to use union instead, but union can only initialize the first member. I had to use struct instead*/
	void    *p_obj;		/* the associated variable */
}       pro[] = {
	{
		"datfile", PRO_STR, {"test.dat",0}, &(config.datfile)
	},
	{
		"recordfile", PRO_STR, {"record",0}, &(config.recordfile)
	},
	{
		"operation", PRO_INT, {"",OVERWRITE}, &(config.operation)  /* READ,WRITE */
	},
	{
		"pagenum", PRO_INT, {"",32}, &(config.pagenum)      
	},
	{
		"pagesize", PRO_INT,{"",512}, &(config.pagesize)   /* unit: B  */
	},
	{
		"BTtestcase", PRO_STR, {"rnd.dat",0}, &(config.BTtestcase)
	},
	{
		"BTdatfile", PRO_STR, {"test.bt",0}, &(config.BTdatfile)
	},
	{
		"BTmetafile", PRO_STR, {"iTable",0}, &(config.BTmetafile)
	},
	{
		"BTrecordfile", PRO_STR, {"btrecord",0}, &(config.BTrecordfile)
	},
	{
		"BTpagesize", PRO_INT, {"",512}, &(config.BTpagesize)  
	},
	{
		"BTrecordnum", PRO_INT, {"",0}, &(config.BTrecordnum)  
	},
	{
		"BTop", PRO_INT, {"",BT_INSERT}, &(config.BTop)  /* BT_INSERT,BT_SEARCH */
	},

	/* whew! */
	{
		0, 0, {"",0}, 0
	}
};

/* forward declaration of private function */
static void     scan_profile( FILE* f );
static void     set_option(char* key, char* val);
static void     set_defaults();


/*
 * loadRCconfig reads  ./.rc and handles arguments
 * given in these files.
 */
void
loadRCconfig()
{
	FILE   *f;
    set_defaults();
	if ((f = fopen(option_source , "r")) != NULL) {
		scan_profile(f);
		(void) fclose(f);
	}
	
}
/*
 *test method for loadRCconfig
 */
void
show_profile()
{
	struct pro *p;

	for (p = pro; p->p_name; p++){
      printf("%s \t",p->p_name);
      switch(p->p_type){
        case PRO_INT:
		  printf("%d", *(int*)p->p_obj);
          break;
        case PRO_STR:
          printf("%s", *(char **)(p->p_obj));
          break;
        default:
		  fprintf(stderr, "set option: internal error: p_type %d\n", p->p_type);
          break;
      }
      printf("\n");
    }
    printf("\n");
}
void
scan_profile(f)
	FILE   *f;
{
	char    line[BUFSIZ];
    char    key[BUFSIZ];
    char    val[BUFSIZ];
    int     lineno = 0;
	
    while (fgets(line,BUFSIZ,f)){
            lineno++;
            /* concern only the first [key,val] pair */
            if(sscanf(line,"%s%s",key,val)==2){
	    
              if(key[0]=='#')   continue;
              if(val[0]=='#'){
    	        fprintf(stderr, "%s: skip \"line %d\": unknown format\n", option_source, lineno);
                continue;
              }        
              set_option(key,val);
            }
	}
}

/*
 * Set the defaults.
 */
void
set_defaults()
{
	struct pro *p;

	for (p = pro; p->p_name; p++){
      switch(p->p_type){
        case PRO_INT:
		  *(int *)p->p_obj = p->p_default.i;
          break;
        case PRO_STR:
		  *(char **)p->p_obj = p->p_default.s;
          break;
        default:
		  fprintf(stderr, "set option: internal error: p_type %d\n", p->p_type);
          break;
      }
    }
}

void
set_option(char   *key, char * val)
{
    struct pro *p;

    for (p = pro; p->p_name; p++)
        if (!strcmp(p->p_name, key))
			goto found;
	fprintf(stderr, "%s: unknown parameter \"%s\"\n", option_source, key);
	exit(1);
found:
	switch (p->p_type) {
      case PRO_INT:
           *(int *)p->p_obj = atoi(val);
           break;
      case PRO_STR:
          /* you must malloc an area for the pro_str type */
          /* 
           if( *(char **)p->p_obj ){
              free ( *(char **)p->p_obj );
           }
           */
           *(char **)p->p_obj= malloc(strlen(val)+1);
           strcpy(*(char**)p->p_obj,val);
           break;
	  default:
			fprintf(stderr, "set option: internal error: p_type %d\n", p->p_type);
			exit(1);
	}
}

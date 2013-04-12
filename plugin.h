#ifndef _PLUGINS_H
#define _PLUGINS_H

#include "utils.h"


enum supported_plugins {
#ifdef _PYTHON_PLUGIN
     PYTHON,
#endif
  
#ifdef _C_PLUGIN
     C,
#endif
    
  END_VALUE
};


const static UNUSED char* supported_plugins_str[] = {
#ifdef _PYTHON_PLUGIN
     "Python",
#endif

#ifdef _C_PLUGIN
     "C",
#endif
 
  NULL
};


const static UNUSED char* plugins_extensions_str[] = {
#ifdef _PYTHON_PLUGIN  
     ".py",
#endif
  
#ifdef _C_PLUGIN      
     ".so",
#endif 

  
  NULL
};


typedef struct _plugin_type 
{
	unsigned int id;
	char* name;
	unsigned short type;
	unsigned short priority;
	void* interpreter;
	struct _plugin_type* next;
	unsigned char state;
} plugin_t;


int proxenet_create_list_plugins(char*);
int proxenet_plugin_list_size();
void proxenet_delete_list_plugins();
void proxenet_print_plugins_list();

#endif /* _PLUGINS_H */

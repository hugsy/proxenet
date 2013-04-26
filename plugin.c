#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "core.h"
#include "utils.h"
#include "plugin.h"
#include "socket.h"
#include "main.h"


/**
 *
 */
char* get_plugin_basename(char* filename, supported_plugins_t type){
	char* name;
	size_t len = -1;

	len = strlen(filename) - 1 - strlen(plugins_extensions_str[type]);
	name = (char*)proxenet_xmalloc(len + 1);
	
	memcpy(name, filename+1, len);
	return name;
}


/**
 *
 */
void proxenet_add_plugin(char* name, supported_plugins_t type, short priority)
{
	plugin_t *cur_ptr, *old_ptr;
	plugin_t *plugin;
	
	plugin 			= (plugin_t*)proxenet_xmalloc(sizeof(plugin_t));
	plugin->id 		= proxenet_plugin_list_size(plugins_list) + 1;
	plugin->filename 	= strdup(name);
	plugin->name		= get_plugin_basename(name, type);
	plugin->type		= type;
	plugin->priority	= priority;
	plugin->next 		= NULL;
	plugin->state		= ACTIVE;

	plugin->interpreter 	= &vms[type];
	plugin->pre_function	= NULL;
	plugin->post_function	= NULL;

	
	/* if first plugin */
	if (!plugins_list) {
		plugins_list = plugin;
		
	} else {
		old_ptr = NULL;
		cur_ptr = plugins_list;
		
		/* browse the plugins list */
		while (true)  {
			
			/* stop if current node priority is lower (i.e. integer higher) than new node's */
			if (cur_ptr->priority > priority) {
				plugin->next = cur_ptr;

				/* if current node is first node, make it first */
				if (!old_ptr) 
					plugins_list = plugin;
				/* otherwise, append */
				else 
					old_ptr->next = plugin;
				break;
			}
			
			/* stop if last element */
			if (cur_ptr->next == NULL) {
				cur_ptr->next = plugin;
				break;
			}
			
			/* otherwise move on to next node */
			old_ptr = cur_ptr;
			cur_ptr = cur_ptr->next; 
		}
	}
	
	if (cfg->verbose > 1)
		xlog(LOG_INFO, "Adding %s plugin '%s'\n", supported_plugins_str[type], plugin->name);
}


/**
 *
 */
int proxenet_plugin_list_size()
{
	plugin_t *p;
	int i;
	
	for (p=plugins_list, i=0; p!=NULL; p=p->next, i++);
	
	return i; 
}



/**
 *
 */
void proxenet_delete_list_plugins()
{
	plugin_t *p = plugins_list;
	plugin_t *next;
	
	while (p != NULL) {
		next = p->next; 
		proxenet_xfree(p->name);
		proxenet_xfree(p->filename);
		proxenet_xfree(p);
		p = next;
	}
}


/**
 *
 */
void proxenet_print_plugins_list() 
{
	plugin_t *p;
	
	sem_wait(&tty_semaphore);
	
	printf("Plugins list:\n");
	for (p = plugins_list; p!=NULL; p=p->next) 
		fprintf(cfg->logfile_fd,
			"|_ priority=%d id=%d type=%s[0x%x] name=%s (%sACTIVE)\n",
			p->priority,
			p->id,
			supported_plugins_str[p->type],
			p->type,
			p->name,
			(p->state==ACTIVE?"":"IN")
		       );
	
	fflush(cfg->logfile_fd);
	sem_post(&tty_semaphore);
}


/**
 *
 */
int proxenet_get_plugin_type(char* filename)
{
	unsigned short type;
	bool is_valid = false;
	char *ext;

	ext = rindex(filename, '.');
	if (!ext)
		return -1;
	
	for (type=0; plugins_extensions_str[type]!=NULL; type++) {
		is_valid = (strcmp(ext, plugins_extensions_str[type])==0);
		if ( is_valid ) {
			return type;
		}
	}
	
	return -1;
}


/**
 * Plugin name structure *MUST* be  `PLUGIN_DIR/<priority><name>.<ext>`
 * with priority in [0, 10[
 * If a file does not match this, it is not added as plugin
 */
int proxenet_create_list_plugins(char* plugin_path)
{ 
	struct dirent *dir_ptr=NULL; 
	DIR *dir = NULL;
	char* plugin_name = NULL;
	short plugin_type=-1, plugin_priority=9;
	int d_name_len;
	
	dir = opendir(plugin_path);
	if (dir == NULL) {
		xlog(LOG_ERROR, "Failed to open '%s': %s\n", plugin_path, strerror(errno));
		return -1;
	}
	
	while ((dir_ptr=readdir(dir))) {
		if (strcmp(dir_ptr->d_name,".")==0) continue;
		if(strcmp(dir_ptr->d_name,"..")==0) continue;
		
		if (atoi(&(dir_ptr->d_name[0])) == 0) continue;
		
		/* plugin name  */ 
		d_name_len = strlen(dir_ptr->d_name);
		if (d_name_len > 510) continue;
		plugin_name = dir_ptr->d_name;
		
		/* plugin type */
		plugin_type = proxenet_get_plugin_type(plugin_name);
		if (plugin_type < 0) continue;
		
		/* plugin priority */
		plugin_priority = (unsigned short)atoi(plugin_name);
		if (plugin_priority>9 || plugin_priority < 1)
			plugin_priority = 9;
		
		/* add plugin in correct place (1: high priority, 9: low priority) */
		proxenet_add_plugin(plugin_name, plugin_type, plugin_priority);
	} 
	
	if (closedir(dir) < 0)
		xlog(LOG_WARNING, "Error while closing '%s': %s\n", plugin_path, strerror(errno));
	
	return 0;
}


/**
 *
 */
int count_plugins_by_type(int type)
{
	int i=0;
	plugin_t* p;
	
	for(p=plugins_list; p!=NULL; p=p->next)
		if (p->type == type) i++;
	
	return i;
}


/**
 *
 */
int count_initialized_plugins_by_type(int type)
{
	int i=0;
	plugin_t* p;
	
	for(p=plugins_list; p!=NULL; p=p->next)
		if (p->type==type && p->interpreter!=NULL) i++;
	
	return i;
}




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <alloca.h>

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
	plugin->id 		= proxenet_plugin_list_size() + 1;
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
unsigned int proxenet_plugin_list_size()
{
	plugin_t *p;
	unsigned int i;

	for (p=plugins_list, i=0; p!=NULL; p=p->next, i++);

	return i;
}



/**
 *
 */
void proxenet_remove_plugin(plugin_t* plugin)
{
	char* name;
	int len;

#ifdef _PERL_PLUGIN
	if(plugin->type == _PERL_) {
		proxenet_xfree(plugin->pre_function);
		proxenet_xfree(plugin->post_function);
	}
#endif

	len = strlen(plugin->name);
	name = alloca(len+1);
	proxenet_xzero(name, len+1);
	strncpy(name, plugin->name, len);

	proxenet_xfree(plugin->name);
	proxenet_xfree(plugin->filename);
	proxenet_xfree(plugin);

	if (cfg->verbose)
		xlog(LOG_INFO, "Plugin '%s' free-ed\n", name);
}



/**
 *
 */
void proxenet_remove_all_plugins()
{
	plugin_t *p = plugins_list;
	plugin_t *next;

	while (p != NULL) {
		next = p->next;
		proxenet_remove_plugin(p);
		p = next;
	}

	plugins_list = NULL;

	if (cfg->verbose)
		xlog(LOG_INFO, "%s\n", "Deleted all plugins");

}


/**
 *
 */
static bool proxenet_build_plugins_list(char *list_str, int *list_len)
{
	plugin_t *p;
	char *ptr;
	int n, max_line_len, total_len;

	max_line_len = 256;
	ptr = list_str;
	n = sprintf(ptr, "Plugins list:\n");
	ptr += n;
	total_len = strlen("Plugins list:\n");

	for (p = plugins_list; p!=NULL; p=p->next) {
		n = snprintf(ptr, max_line_len,
			     "|_ priority=%-3d id=%-3d type=%-10s[0x%x] name=%-20s (%sACTIVE)\n",
			     p->priority,
			     p->id,
			     supported_plugins_str[p->type],
			     p->type,
			     p->name,
			     (p->state==ACTIVE?"":"IN")
			    );


		if (n<0) {
			xlog(LOG_ERROR, "Error: %s\n", strerror(errno));
			return false;
		}

		ptr += n;
		total_len += n;

		if ((ptr - list_str + max_line_len) > *list_len) {
			xlog(LOG_ERROR, "%s\n", "Overflow detected");
			return false;
		}
	}

	*list_len = total_len;

	return true;
}


/**
 *
 */
void proxenet_print_plugins_list(int fd)
{
	char *list_str;
	int list_len;

	list_len = 2048;
	list_str = (char*)alloca(list_len);
	memset(list_str, 0, list_len);

	if (!proxenet_build_plugins_list(list_str, &list_len)) {
		xlog(LOG_ERROR, "%s\n", "Failed to build plugins list string");
		return;
	}

	if (fd<0)
		xlog(LOG_INFO, "%s", list_str);
	else {
		proxenet_write(fd, list_str, list_len);
		proxenet_write(fd, "\n", 1);
	}

}


/**
 *
 */
int proxenet_get_plugin_type(char* filename)
{
	unsigned short type;
	bool is_valid = false;
	char *ext;

	ext = strrchr(filename, '.');
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
 * with priority in [1, 9]
 * If a file does not match this pattern, it will be discarded
 *
 * @param plugin_path is the path to look for plugins
 * @param plugin_name is the name of the plugin to add. If NULL, this function will try
 *        to *all* the files in the directory.
 * @return 0 on success, -1 on error
 */
int proxenet_add_new_plugins(char* plugin_path, char* plugin_name)
{
	struct dirent *dir_ptr=NULL;
	DIR *dir = NULL;
	char* name = NULL;
	short type=-1, priority=9;
	int d_name_len;
        bool add_all = (plugin_name==NULL)?true:false;

#ifdef DEBUG
        if (add_all)
                xlog(LOG_DEBUG, "Trying to add all files in '%s'\n", plugin_path);
        else
                xlog(LOG_DEBUG, "Trying to add '%s/%s'\n", plugin_path, plugin_name);
#endif
	dir = opendir(plugin_path);
	if (dir == NULL) {
		xlog(LOG_ERROR, "Failed to open '%s': %s\n", plugin_path, strerror(errno));
		return -1;
	}

	while ((dir_ptr=readdir(dir))) {
		if (strcmp(dir_ptr->d_name,".")==0)
                        continue;

		if (strcmp(dir_ptr->d_name,"..")==0)
                        continue;

                /* if add one plugin, loop until the right name */
                if (!add_all && strcmp(dir_ptr->d_name, plugin_name)!=0)
                        continue;

                /* if first char is not valid int, continue */
		if (atoi(&(dir_ptr->d_name[0])) == 0)
                        continue;

		/* plugin name  */
		d_name_len = strlen(dir_ptr->d_name);
		if (d_name_len > 510) continue;
		name = dir_ptr->d_name;

		/* plugin priority (discard if invalid) */
		priority = (unsigned short)atoi(name);
		if ( !(1 <= priority && priority <= 9) )
			continue;

		/* plugin type */
		type = proxenet_get_plugin_type(name);
		if (type < 0)
                        continue;

		/* add plugin in correct place (1: high priority, 9: low priority) */
		proxenet_add_plugin(name, type, priority);

                /* if add one plugin only, there is no need to keep looping */
                if (!add_all)
                        break;
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
		if (p->type  ==type &&
		    p->state == ACTIVE &&
		    p->interpreter!=NULL) i++;

	return i;
}

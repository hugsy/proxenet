#include <alloca.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "core.h"
#include "utils.h"
#include "plugin.h"
#include "socket.h"
#include "main.h"


/**
 *
 */
static char* plugin_basename(char* dst, char* src, supported_plugins_t type){
	size_t len;
	len = strlen(src) - strlen(plugins_extensions_str[type]);
        return memcpy(dst, src, len);
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
	plugin->type		= type;
	plugin->priority	= priority;
	plugin->next 		= NULL;
	plugin->state		= ACTIVE;

	plugin->interpreter 	= &vms[type];
	plugin->pre_function	= NULL;
	plugin->post_function	= NULL;


        if (!strcpy(plugin->filename, name)){
                xlog(LOG_CRITICAL, "strcpy() failed for '%s': %s", name, strerror(errno));
                abort();
        }

        if (snprintf(plugin->fullpath, sizeof(plugin->fullpath), "%s/%s", cfg->plugins_path, name) < 0){
                xlog(LOG_CRITICAL, "snprintf() failed for '%s/%s': %s", cfg->plugins_path, name, strerror(errno));
                abort();
        }

        if (!plugin_basename(plugin->name, name, type)){
                xlog(LOG_CRITICAL, "plugin_basename() failed for '%s': %s", name, strerror(errno));
                abort();
        }

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
 * This function zero-es out the block allocated for a specific plugin.
 */
void proxenet_free_plugin(plugin_t* plugin)
{
	char name[PATH_MAX] = {0, };
	int len;

#ifdef _PERL_PLUGIN
	if(plugin->type == _PERL_) {
		proxenet_xfree(plugin->pre_function);
		proxenet_xfree(plugin->post_function);
	}
#endif

	len = strlen(plugin->filename);
	strncpy(name, plugin->filename, len);


	proxenet_xfree(plugin);

#ifdef DEBUG
        xlog(LOG_DEBUG, "Free-ed plugin '%s'\n", name);
#endif

        return;
}



/**
 * This function zero-es out all the blocks allocated for plugins and
 * NULL-s the plugin_list pointer.
 */
void proxenet_free_all_plugins()
{
	plugin_t *p = plugins_list;
	plugin_t *next;

	while (p != NULL) {
		next = p->next;
		proxenet_free_plugin(p);
		p = next;
	}

	plugins_list = NULL;

#ifdef DEBUG
        xlog(LOG_DEBUG, "%s\n", "Plugins list is free-ed");
#endif

        return;
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

        return;
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
 * Search if a loaded plugin has a name matching the argument
 *
 * @param name : plugin name to look up
 * @return true if a match is found, false in any other case
 */
static bool is_plugin_loaded(char* name)
{
	plugin_t* p;
	for(p=plugins_list; p!=NULL; p=p->next){
                if(!strcmp(p->filename, name)){
                        return true;
                }
        }

        return false;
}


/**
 * Print all plugins available in the plugin directory
 *
 * @param fd file descriptor to write output on
 */
void proxenet_print_all_plugins(int fd)
{
        struct dirent *dir_ptr=NULL;
	DIR *dir = NULL;
	char* name = NULL;
        char msg[2048] = {0, };
        int n, type;

        dir = opendir(cfg->plugins_path);
	if (dir == NULL) {
		xlog(LOG_ERROR, "Failed to open '%s': %s\n", cfg->plugins_path, strerror(errno));
                n = snprintf(msg, sizeof(msg), "Error while opening dir '%s': %s\n", cfg->plugins_path, strerror(errno));
                proxenet_write(fd, msg, n);
		return;
	}

        n = snprintf(msg, sizeof(msg),
                     "Enumerating all plugins in "BLUE"%s"NOCOLOR"\n",
                     cfg->plugins_path);
        proxenet_write(fd, msg, n);

	while ((dir_ptr=readdir(dir))) {
                type = -1;
                name = dir_ptr->d_name;
		if (!strcmp(name,".") || !strcmp(name,".."))
                        continue;

                type = proxenet_get_plugin_type(name);
		if (type < 0)
                        continue;

                if (is_plugin_loaded(name)) {
                        n = snprintf(msg, sizeof(msg),
                                     "[*] "GREEN"%s"NOCOLOR" ("RED"%s"NOCOLOR") - already loaded\n",
                                     name, supported_plugins_str[type]);
                } else {
                        n = snprintf(msg, sizeof(msg),
                                     "[*] "CYAN"%s"NOCOLOR" ("RED"%s"NOCOLOR")\n",
                                     name, supported_plugins_str[type]);
                }

                proxenet_write(fd, msg, n);
                proxenet_xzero(msg, sizeof(msg));
        }

        if (closedir(dir) < 0){
		xlog(LOG_ERROR, "Failed to close '%s': %s\n", cfg->plugins_path, strerror(errno));
                n = snprintf(msg, sizeof(msg), "Error while closing dir '%s': %s\n", cfg->plugins_path, strerror(errno));
                proxenet_write(fd, msg, n);
		return;
        }

        return;
}


/**
 * Plugin name structure *MUST* be  `PLUGIN_DIR/[<priority>]<name>.<ext>`
 * <priority> is an int in [1, 9]. If <priority> is found as the first char of the
 * file name, it will be applied to the plugin. If no priority is specify, a default
 * priority will be applied.
 *
 * If a file does not match this pattern, it will be discarded
 *
 * @param plugin_path is the path to look for plugins
 * @param plugin_name is the name of the plugin to add. If NULL, this function will try
 *        to *all* the files in the directory.
 * @return the number of added plugins on success, -1 on error
 */
int proxenet_add_new_plugins(char* plugin_path, char* plugin_name)
{
	struct dirent *dir_ptr=NULL;
	DIR *dir = NULL;
	char* name = NULL;
	short type=-1, priority;
	int d_name_len;
        bool add_all = (plugin_name==NULL)?true:false;
        unsigned int nb_plugin_added = 0;

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
#ifdef DEBUG
                xlog(LOG_DEBUG, "File '%s/%s'\n", plugin_path, dir_ptr->d_name);
#endif

                /* if add one plugin, loop until the right name */
                if (!add_all && strcmp(dir_ptr->d_name, plugin_name)!=0)
                        continue;

                if (dir_ptr->d_type == DT_LNK){
                        /* if entry is a symlink, ensure it's pointing to a file in plugins_path */
                        ssize_t l = -1;
                        char fullpath[PATH_MAX] = {0, };
                        char realpath_buf[PATH_MAX] = {0, };

                        l = snprintf(fullpath, PATH_MAX, "%s/%s", plugin_path, dir_ptr->d_name);
                        if(l == -1){
			        xlog(LOG_ERROR, "snprintf() failed on '%s'\n",
				     dir_ptr->d_name ,
				     errno,
				     strerror(errno));
				continue;
			}

                        if(!realpath(fullpath, realpath_buf)){
                                xlog(LOG_ERROR, "realpath failed on '%s': %d - %s\n",
                                     fullpath,
                                     errno,
                                     strerror(errno));
                                continue;
                        }

                        l = strlen(cfg->plugins_path);

                        if( strncmp(realpath_buf, cfg->plugins_path, l) != 0 )
                                continue;

                } else {
                        /* if not a symlink nor regular file, continue */
                        if (dir_ptr->d_type != DT_REG)
                                continue;
                }

                /* if first char is valid integer, this will be the plugin priority */
		if (atoi(&(dir_ptr->d_name[0])) > 0)
                        priority = (unsigned short)atoi(&(dir_ptr->d_name[0]));
                else
                        priority = (unsigned short) CFG_DEFAULT_PLUGIN_PRIORITY;

		/* plugin name  */
		d_name_len = strlen(dir_ptr->d_name);
		if (d_name_len > 510) continue;
		name = dir_ptr->d_name;

		/* plugin type */
		type = proxenet_get_plugin_type(name);
		if (type < 0)
                        continue;

		/* add plugin in correct place (1: high priority, 9: low priority) */
		proxenet_add_plugin(name, type, priority);

		nb_plugin_added++;

		/* if add one plugin only, there is no need to keep looping */
		if (!add_all)
			break;
	}

	if (closedir(dir) < 0){
		xlog(LOG_WARNING, "Error while closing '%s': %s\n", plugin_path, strerror(errno));
        }

	return nb_plugin_added;
}


/**
 *
 */
unsigned int count_plugins_by_type(supported_plugins_t type)
{
	unsigned int i=0;
	plugin_t* p;

	for(p=plugins_list; p!=NULL; p=p->next)
		if (p->type == type) i++;

	return i;
}


/**
 *
 */
unsigned int count_initialized_plugins_by_type(supported_plugins_t type)
{
	unsigned int i=0;
	plugin_t* p;

	for(p=plugins_list; p!=NULL; p=p->next)
		if (p->type  == type &&
		    p->state == ACTIVE &&
		    p->interpreter!=NULL) i++;

	return i;
}

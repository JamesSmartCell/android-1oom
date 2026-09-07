#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "os.h"
#include "lib.h"
#include "util.h"
#include "types.h"

const struct cmdline_options_s os_cmdline_options[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

#define NUM_ALL_DATA_PATHS 8

static char *data_path = NULL;
static char *user_path = NULL;
static char *all_data_paths[NUM_ALL_DATA_PATHS] = { NULL };
static int num_data_paths = 0;

const char *idstr_os = "android";

int os_early_init(void)
{
    return 0;
}

int os_init(void)
{
    return 0;
}

void os_shutdown(void)
{
    lib_free(user_path);
    user_path = NULL;
    lib_free(data_path);
    data_path = NULL;
    for (int i = 0; i < num_data_paths; ++i) {
        lib_free(all_data_paths[i]);
        all_data_paths[i] = NULL;
    }
    num_data_paths = 0;
}

const char **os_get_paths_data(void)
{
    if (num_data_paths == 0) {
        int i = 0;
        if (data_path) {
            all_data_paths[i++] = lib_stralloc(data_path);
        }
        all_data_paths[i++] = lib_stralloc(".");
        all_data_paths[i] = NULL;
        num_data_paths = i;
    }
    return (const char **)all_data_paths;
}

const char *os_get_path_data(void)
{
    return data_path;
}

void os_set_path_data(const char *path)
{
    lib_free(data_path);
    data_path = lib_stralloc(path);
}

const char *os_get_path_user(void)
{
    if (user_path == NULL) {
        user_path = lib_stralloc(data_path ? data_path : ".");
    }
    return user_path;
}

void os_set_path_user(const char *path)
{
    lib_free(user_path);
    user_path = lib_stralloc(path);
}

int os_make_path(const char *path)
{
    if ((path == NULL) || ((path[0] == '.') && (path[1] == '\0'))) {
        return 0;
    }
    if (access(path, F_OK)) {
        return mkdir(path, 0700);
    }
    return 0;
}

int os_make_path_user(void)
{
    return os_make_path(os_get_path_user());
}

int os_make_path_for(const char *filename)
{
    int res = 0;
    char *path;
    util_fname_split(filename, &path, NULL);
    if (path != NULL) {
        res = os_make_path(path);
        lib_free(path);
    }
    return res;
}

const char *os_get_fname_save_slot(char *buf, size_t bufsize, int savei)
{
    (void)buf;
    (void)bufsize;
    (void)savei;
    return NULL;
}

const char *os_get_fname_save_year(char *buf, size_t bufsize, int year)
{
    (void)buf;
    (void)bufsize;
    (void)year;
    return NULL;
}

const char *os_get_fname_cfg(char *buf, size_t bufsize, const char *gamestr, const char *uistr, const char *hwstr)
{
    (void)buf;
    (void)bufsize;
    (void)gamestr;
    (void)uistr;
    (void)hwstr;
    return NULL;
}

const char *os_get_fname_log(char *buf, size_t bufsize)
{
    if (buf) {
        lib_strcpy(buf, "1oom_log.txt", bufsize);
        return buf;
    }
    return "1oom_log.txt";
}

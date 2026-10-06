/*
 * tb_script_limits.c - exercise configurable shell limits and script isolation.
 * SPDX-License-Identifier: MIT
 */
#include "tdsh.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

_Static_assert(TDSH_MAX_VARS == EXPECT_VARS, "variable override ignored");
_Static_assert(TDSH_VAR_NAME_MAX == EXPECT_NAME, "name override ignored");
_Static_assert(TDSH_VAR_VALUE_MAX == EXPECT_VALUE, "value override ignored");
_Static_assert(TDSH_SCRIPT_TASK_STACK == EXPECT_STACK, "stack override ignored");

static int s_failures;
static int s_probe_calls;
static size_t s_requested_stack;
static size_t s_job_bytes;

#define CHECK(condition)                                                 \
    do                                                                   \
    {                                                                    \
        if (!(condition))                                                \
        {                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
            ++s_failures;                                                \
        }                                                                \
    } while (0)

static int s_worker_run(void *context, const char *name, size_t stack_bytes,
                        int priority, bool background, tdsh_worker_fn_t worker,
                        void *arg, tdsh_worker_cleanup_fn_t cleanup, int *result)
{
    (void)context;
    (void)name;
    (void)priority;
    CHECK(!background);
    s_requested_stack = stack_bytes;
    tdsh_memory_stats_t stats;
    tdsh_memory_get_stats(&stats);
    s_job_bytes = stats.live_bytes;
    *result = worker(arg);
    cleanup(arg);
    return 0;
}

static int s_probe(tdsh_session_t *session, int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const char *value = tdsh_var_get(session, "INHERITED");
    CHECK(value && strcmp(value, "parent") == 0);
    CHECK(!session->interactive);
    ++s_probe_calls;
    CHECK(tdsh_var_set(session, "INHERITED", "child") == 0);
    CHECK(tdsh_var_set(session, "LOCAL", "script") == 0);
    return 0;
}

int main(void)
{
    char root_template[] = "/tmp/tang-script-limits-XXXXXX";
    char *root = mkdtemp(root_template);
    CHECK(root != NULL);
    if (!root)
        return 1;

    static const tdsh_platform_api_t platform = {.worker_run = s_worker_run};
    tdsh_core_config_t config = TDSH_CORE_CONFIG_DEFAULT();
    config.fs_root = root;
    config.platform = &platform;
    CHECK(tdsh_core_init(&config) == 0);

    tdsh_session_t parent;
    CHECK(tdsh_session_init(&parent, "root", true) == 0);
    CHECK(tdsh_var_set(&parent, "INHERITED", "parent") == 0);
    static const tdsh_command_t command = {"probe", "probe", "probe", s_probe, 0};
    CHECK(tdsh_register_command(&command) == 0);

    char path[TDSH_MAX_REAL_PATH];
    CHECK(snprintf(path, sizeof(path), "%s/probe.tdsh", root) < (int)sizeof(path));
    FILE *file = fopen(path, "w");
    CHECK(file != NULL);
    if (file)
    {
        fputs("#!/bin/tdsh\nprobe\nreturn 0\n", file);
        fclose(file);
        for (int iteration = 0; iteration < 4; ++iteration)
        {
            CHECK(tdsh_run_script(&parent, "/probe.tdsh", false) == 0);
            CHECK(strcmp(tdsh_var_get(&parent, "INHERITED"), "parent") == 0);
            CHECK(tdsh_var_get(&parent, "LOCAL") == NULL);
            tdsh_memory_stats_t stats;
            tdsh_memory_get_stats(&stats);
            CHECK(stats.live_blocks == 0 && stats.live_bytes == 0);
        }
        CHECK(s_probe_calls == 4);
        CHECK(s_requested_stack == TDSH_SCRIPT_TASK_STACK);
        unlink(path);
    }

    tdsh_session_t limits;
    CHECK(tdsh_session_init(&limits, "root", false) == 0);
    char name[TDSH_VAR_NAME_MAX + 1];
    char value[TDSH_VAR_VALUE_MAX + 1];
    memset(name, 'n', sizeof(name));
    name[TDSH_VAR_NAME_MAX - 1] = '\0';
    memset(value, 'v', sizeof(value));
    value[TDSH_VAR_VALUE_MAX - 1] = '\0';
    CHECK(tdsh_var_set(&limits, name, value) == 0);
    value[TDSH_VAR_VALUE_MAX - 1] = 'v';
    value[TDSH_VAR_VALUE_MAX] = '\0';
    CHECK(tdsh_var_set(&limits, name, value) == -ENOSPC);
    CHECK(strlen(tdsh_var_get(&limits, name)) == TDSH_VAR_VALUE_MAX - 1);
    CHECK(tdsh_var_unset(&limits, name) == 0);
    name[TDSH_VAR_NAME_MAX - 1] = 'n';
    name[TDSH_VAR_NAME_MAX] = '\0';
    CHECK(tdsh_var_set(&limits, name, "short") == -ENOSPC);

    for (int index = 0; index < TDSH_MAX_VARS; ++index)
    {
        char key[16];
        snprintf(key, sizeof(key), "V%d", index);
        CHECK(tdsh_var_set(&limits, key, "value") == 0);
    }
    CHECK(tdsh_var_set(&limits, "EXTRA", "value") == -ENOSPC);
    CHECK(tdsh_var_set(&limits, "V0", "updated") == 0);
    CHECK(tdsh_var_unset(&limits, "V0") == 0);
    CHECK(tdsh_var_set(&limits, "EXTRA", "value") == 0);

    rmdir(root);
    printf("session=%zu table=%zu job=%zu requested-stack=%zu: %s\n",
           sizeof(parent), sizeof(parent.vars), s_job_bytes, s_requested_stack,
           s_failures ? "FAIL" : "PASS");
    return s_failures ? 1 : 0;
}

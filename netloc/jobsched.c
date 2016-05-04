/*
 * Copyright © 2016 Inria.  All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 * See COPYING in top-level directory.
 *
 * $HEADER$
 */

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdlib.h>

#include <private/autogen/config.h>
#include <netloc.h>

#ifdef HWLOC_HAVE_SLURM
#include <slurm/slurm.h>
#endif

int netloc_get_current_nodes(int *pnum_nodes, char ***pnodes)
{
    int num_nodes;
    char **nodes;
    int ret;

    /* We try different resource managers */
    int (*get_node_functions[2])(int *, char ***) = {
        slurm_get_current_nodes,
        pbs_get_current_nodes
    };

    int num_functions = sizeof(get_node_functions)/sizeof(void *);

    int f;
    for (f = 0; f < num_functions; f++) {
        ret = get_node_functions[f](&num_nodes, &nodes);
        if (ret == NETLOC_SUCCESS)
            break;
    }
    if (f == num_functions) {
        fprintf(stderr, "Error: your resource manager is not compatible\n");
        return NETLOC_ERROR;
    }

    *pnum_nodes = num_nodes;
    *pnodes = nodes;
    return NETLOC_SUCCESS;
}

/******************************************************************************/
/* Handles SLURM job manager */
int slurm_get_current_nodes(int *pnum_nodes, char ***pnodes)
{
#ifdef HWLOC_HAVE_SLURM
    char *nodelist = getenv("SLURM_NODELIST");
    char *domainname = getenv("NETLOC_DOMAINNAME");

    if (nodelist) {
        hostlist_t hostlist = slurm_hostlist_create(nodelist);
        int num_nodes = slurm_hostlist_count(hostlist);
        int by_node;
        slurm_get_proc_number(&by_node);
        int total_num_nodes = num_nodes*by_node;

        int remove_length;
        if (!domainname)
            remove_length = 0;
        else
            remove_length = strlen(domainname);

        char **nodes = (char **)malloc(total_num_nodes*sizeof(char *));

        for (int n = 0; n < num_nodes; n++) {
            char *hostname = slurm_hostlist_shift(hostlist);
            for (int i = 0; i < by_node; i++) {
                nodes[by_node*n+i] = strndup(hostname, strlen(hostname)-remove_length);
            }
            free(hostname);
        }
        slurm_hostlist_destroy(hostlist);

        *pnum_nodes = total_num_nodes;
        *pnodes = nodes;

        return NETLOC_SUCCESS;
    }
#endif // HWLOC_HAVE_SLURM
    return NETLOC_ERROR;
}

int slurm_get_proc_number(int *pnum_ppn)
{
    char *variable = getenv("SLURM_JOB_CPUS_PER_NODE");
    if (variable) {
        *pnum_ppn = atoi(variable);
        return NETLOC_SUCCESS;
    } else {
        printf("Error: problem in your SLURM environment\n");
        return NETLOC_ERROR;
    }
}


/******************************************************************************/
/* Handles PBS job manager */
int pbs_get_current_nodes(int *pnum_nodes, char ***pnodes)
{
    char *pbs_file = getenv("PBS_NODEFILE");
    char *domainname = getenv("NETLOC_DOMAINNAME");
    char *line = NULL;
    size_t size = 0;

    if (pbs_file) {
        FILE *node_file = fopen(pbs_file, "r");

        int num_nodes = atoi(getenv("PBS_NUM_NODES"));
        int by_node;
        pbs_get_proc_number(&by_node);

        int total_num_nodes = num_nodes*by_node;
        int found_nodes = 0;
        char **nodes = (char **)malloc(total_num_nodes*sizeof(char *));

        int remove_length;
        if (!domainname)
            remove_length = 0;
        else
            remove_length = strlen(domainname);

        while (getline(&line, &size, node_file) > 0) {
            if (found_nodes > total_num_nodes) {
                printf("Error: variable PBS_NUM_NODES does not correspond to "
                        "value from PBS_NODEFILE\n");
                return NETLOC_ERROR;
            }
            nodes[found_nodes] = strndup(line, strlen(line)-remove_length-1);
            found_nodes++;
        }
        *pnodes = nodes;
        *pnum_nodes = total_num_nodes;
    } else { // TODO
        // you do not use PBS resource manager
        return NETLOC_ERROR;
    }

    return NETLOC_SUCCESS;
}


int pbs_get_proc_number(int *pnum_ppn)
{
    char *variable = getenv("PBS_NUM_PPN");
    if (variable) {
        *pnum_ppn = atoi(variable);
        return NETLOC_SUCCESS;
    } else {
        printf("Error: problem in your PBS environment\n");
        return NETLOC_ERROR;
    }
}


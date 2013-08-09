#include "config.h"
#include "xyzsh.h"
#include <stdio.h>
#include <string.h>

static void version()
{
    printf("xyzsh version %s\n", getenv("XYZSH_VERSION"));
}

static void main_xyzsh_job_done(int job_num, char* job_title)
{
    printf("%d: %s done.\n", job_num+1, job_title);
}

int main(int argc, char** argv)
{
    CHECKML_BEGIN();
/*
printf("sObject %d\n", (int)sizeof(sObject));
printf("string_obj %d\n", (int)sizeof(string_obj));
printf("vector_obj %d\n", (int)sizeof(vector_obj));
printf("hash_obj %d\n", (int)sizeof(hash_obj));
printf("list_obj %d\n", (int)sizeof(list_obj));
printf("fd_obj %d\n", (int)sizeof(fd_obj));
printf("fd2_obj %d\n", (int)sizeof(fd2_obj));
printf("job_obj %d\n", (int)sizeof(job_obj));
printf("block_obj %d\n", (int)sizeof(struct block_obj));
printf("uobject_obj %d\n", (int)sizeof(uobject_obj));
printf("nfun_obj %d\n", (int)sizeof(nfun_obj));
printf("fun_obj %d\n", (int)sizeof(fun_obj));
printf("class_obj %d\n", (int)sizeof(class_obj));
printf("completion_obj %d\n", (int)sizeof(completion_obj));
printf("external_prog_obj %d\n", (int)sizeof(external_prog_obj));
printf("external_obj %d\n", (int)sizeof(external_obj));
*/
    srandom((unsigned)time(NULL));

    char* optc = NULL;
    BOOL no_runtime_script = FALSE;
    BOOL curses = FALSE;
    char* file = NULL;
    int i;
    for(i=1; i<argc; i++) {
        if(strcmp(argv[i], "-c") == 0 && i+1 < argc) {
            optc = argv[i+1];
        }
        else if(strcmp(argv[i], "--version") == 0) {
            version();
            exit(0);
        }
        else if(strcmp(argv[i], "-cr") == 0) {
            curses = TRUE;
        }
        else if(strcmp(argv[i], "-nr") == 0) {
            no_runtime_script = TRUE;
        }
        else {
            if(file == NULL) file = argv[i];
        }
    }

    if(optc) {
        xyzsh_init(kATOptC, TRUE);
        xyzsh_opt_c(optc, argv, argc);
    }
    else if(file) {
        xyzsh_init(kATOptC, TRUE);
        (void)xyzsh_load_file(file, argv, argc, gRootObject);
    }
    else if(curses) {
        xyzsh_init(kATConsoleApp, no_runtime_script);
        xyzsh_readline_interface_on_curses("", -1, argv, argc, FALSE, TRUE);
    }
    else {
        xyzsh_init(kATConsoleApp, no_runtime_script);
        xyzsh_readline_interface("", -1, argv, argc, FALSE, TRUE);
        xyzsh_kill_all_jobs();
    }

    xyzsh_final();

    CHECKML_END();

    return 0;
}

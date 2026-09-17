#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>

typedef struct {    //se define la estructura Comando
    char **argv;
    char *infile;
    char *outfile;
    int   append;
} Comando;

void mostrar_prompt(void){
    char directorio[PATH_MAX];

    if(getcwd(directorio, sizeof(directorio)) == NULL){
        perror("getcwd");
        return;
    }
    printf("mishell:%s$ ", directorio);
    fflush(stdout);
}

char **separar_tokens(char *linea){
    int capacidad = 10; //numero de punteros que se pueden guardar al inicio en el arreglo de tokens
    int cantidad = 0; //numero de tokens encontrados

    char **tokens = malloc(capacidad*sizeof(char *));
    
    if(tokens == NULL){
        perror("malloc");
        return NULL;
    }

    char *token = strtok(linea, " \t"); //strtok recorre la línea buscando tokens.

    while(token != NULL){
        if(cantidad >= capacidad-1){
            capacidad = capacidad*2;
            char **tokensNuevos = realloc(tokens, capacidad*sizeof(char *));

            if(tokensNuevos == NULL){
                perror("realloc");
                free(tokens);
                return NULL;
            }

            tokens = tokensNuevos;
        }

        tokens[cantidad] = token;
        cantidad++;

        token = strtok(NULL, " \t");
    }

    tokens[cantidad] = NULL;
    return tokens;

}
//se añade la funcion que separa la línea en comandos
Comando *construir_pipeline(char **tokens, int *n_comandos) {
    int n = 1;
    for (int i = 0; tokens[i] != NULL; i++)
        if (strcmp(tokens[i], "|") == 0) n++;

    Comando *comandos = malloc(n * sizeof(Comando));
    for (int i = 0; i < n; i++) {
        comandos[i].argv = NULL;
        comandos[i].infile = NULL;
        comandos[i].outfile = NULL;
        comandos[i].append = 0;
    }

    int idx = 0, cap = 10, cnt = 0;
    char **argv_actual = malloc(cap * sizeof(char *));

    for (int i = 0; tokens[i] != NULL; i++) {
        char *tok = tokens[i];

        if (strcmp(tok, "|") == 0) {
            argv_actual[cnt] = NULL;
            comandos[idx++].argv = argv_actual;
            cap = 10; cnt = 0;
            argv_actual = malloc(cap * sizeof(char *));
            continue;
        }
        if (strcmp(tok, "<") == 0) {
            if (tokens[++i] == NULL) { fprintf(stderr, "error: falta archivo tras '<'\n"); return NULL; }
            comandos[idx].infile = tokens[i];
            continue;
        }
        if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            int append = (strcmp(tok, ">>") == 0);
            if (tokens[++i] == NULL) { fprintf(stderr, "error: falta archivo tras '%s'\n", tok); return NULL; }
            comandos[idx].outfile = tokens[i];
            comandos[idx].append = append;
            continue;
        }

        if (cnt >= cap - 1) { cap *= 2; argv_actual = realloc(argv_actual, cap * sizeof(char *)); }
        argv_actual[cnt++] = tok;
    }

    argv_actual[cnt] = NULL;
    comandos[idx].argv = argv_actual;

    *n_comandos = n;
    return comandos;
}


//se añade la función que se encargará de ejecutar el pipeline
int ejecutar_pipeline(Comando *comandos, int n) {
    int (*pipes)[2] = NULL;
    if (n > 1) {
        pipes = malloc((n - 1) * sizeof(*pipes));
        for (int i = 0; i < n - 1; i++) {
            if (pipe(pipes[i]) == -1) { perror("pipe"); return -1; }
        }
    }

    pid_t *pids = malloc(n * sizeof(pid_t));

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return -1; }

        if (pid == 0) {
            if (i > 0)      dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < n - 1)  dup2(pipes[i][1], STDOUT_FILENO);

            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            if (comandos[i].infile) {
                int fd = open(comandos[i].infile, O_RDONLY);
                if (fd == -1) { perror("open"); _exit(EXIT_FAILURE); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (comandos[i].outfile) {
                int flags = O_WRONLY | O_CREAT | (comandos[i].append ? O_APPEND : O_TRUNC);
                int fd = open(comandos[i].outfile, flags, 0644);
                if (fd == -1) { perror("open"); _exit(EXIT_FAILURE); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            execvp(comandos[i].argv[0], comandos[i].argv);
            perror("execvp");
            _exit(EXIT_FAILURE);
        }
        pids[i] = pid;
    }

    if (n > 1) {
        for (int j = 0; j < n - 1; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
    }

    int estado;
    for (int i = 0; i < n; i++) waitpid(pids[i], &estado, 0);

    free(pipes);
    free(pids);
    return 0;
}


int main(void){
    char linea[4096]; //lugar en el que se guardará la línea que ingrese el usuario

    //el ciclo a continuación se repetirá continuamente, debido a que es algo 
    //que debe realizar la shell en todo momento que esté activa.
    while(1){
        mostrar_prompt();
        
        if(fgets(linea, sizeof(linea), stdin) == NULL){
            printf("\n");
            break;
        }

        linea[strcspn(linea, "\n")] = '\0';

        char **argv = separar_tokens(linea); //separa la linea en tokens

        if(argv == NULL){
            return EXIT_FAILURE;
        }

        if(argv[0] == NULL){
            free(argv);
            continue;
        }

        if(strcmp(argv[0], "cd") == 0){
            if(argv[1] == NULL){
                char *home = getenv("HOME");

                if(home == NULL){
                    fprintf(stderr, "cd: no se ha definido HOME\n");
                }
                else if (chdir(home) == -1){
                    perror("cd");
                }
            }
            else{
                if(chdir(argv[1]) == -1){
                    perror("cd");
                }
            }

            free(argv);
            continue;
        }

        if(strcmp(argv[0], "exit") == 0){
            int cod = 0;
            if(argv[1] != NULL){
                cod = atoi(argv[1]);
            }

            free(argv);
            exit(cod);
        }

        //jobs == depende de R5

        //pmon == depende de la seccion 3

        int n_comandos;
        Comando *comandos = construir_pipeline(argv, &n_comandos);
        free(argv);

        if (comandos != NULL) {
            ejecutar_pipeline(comandos, n_comandos);
            for (int i = 0; i < n_comandos; i++) free(comandos[i].argv);
            free(comandos);
        }

    }
    return 0;
}
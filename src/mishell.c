#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>

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

        pid_t pid = fork();



        if(pid < 0){
            perror("fork");
            free(argv);
            return EXIT_FAILURE;
        }

        if(pid == 0){
            execvp(argv[0], argv);
            perror("execvp");
            _exit(EXIT_FAILURE);
        }
        else{
            int estado;
            if(waitpid(pid, &estado, 0) == -1){
                perror("waitpid");
                free(argv);
                return EXIT_FAILURE;
            }
        }
        free(argv);

    }
    return 0;
}
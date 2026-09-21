#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

typedef struct {    //se define la estructura Comando
    char **argv;
    char *infile;
    char *outfile;
    int   append;
} Comando;


#define MAX_JOBS  64
#define MAX_PIPELINE  32

typedef struct 
{
    int en_uso;// 1 si el slot está ocupado por un job
    int id;//nro de job mostrado al usuario
    pid_t pids[MAX_PIPELINE];//pids de todos los procesos del job
    int n_pids;//cantidad total de procesos del job
    int n_vivos;//cuantos de esos procesos siguen vivos
    volatile sig_atomic_t terminado;//puesto en 1 por el manejador de SIGCHILD
    int notificado;//1 una vez que ya se avisó "Done" al usuario
    char comando[256];//texto del comando, para jobs/pmon/aviso final
} Job;


static Job jobs_bg[MAX_JOBS];
static int siguiente_job_id = 1;

/*Bandera global: se pone en 1 dentro del manejador de SIGCHILD cuando
al menos un job terminó por completo. Se revisa en el loop principal antes
de mostrar el prompt*/
static volatile sig_atomic_t hay_jobs_terminados = 0;

void mostrar_prompt(void){
    char directorio[PATH_MAX];

    if(getcwd(directorio, sizeof(directorio)) == NULL){
        perror("getcwd");
        return;
    }
    printf("mishell:%s$ ", directorio);
    fflush(stdout);
}


/*Utilidades para proteger el acceso a jobs_bg contra el manejador
de SIGCHILD (bloquean/desbloquean unicamente esa señal)*/
static void bloquear_sigchld(sigset_t *mascara_anterior){
    sigset_t bloqueo;
    sigemptyset(&bloqueo);
    sigaddset(&bloqueo, SIGCHLD);
    sigprocmask(SIG_BLOCK, &bloqueo, mascara_anterior);
}

static void restaurar_mascara(const sigset_t *mascara_anterior){
    sigprocmask(SIG_SETMASK, mascara_anterior,NULL);
}

/*Busca un slot libre en jobs_bg y registra ahí un nuevo job en
background con los pids entregados. Devuelve el id asignado o -1 si no hay espacio.
Se bloquea SIGCHILD mientras se escribe la tabla para que el manejador no 
la lea a medio construir*/
int registrar_job(pid_t *pids, int n, const char *texto_comando){
    sigset_t anterior;
    bloquear_sigchld(&anterior);
    
    int slot = -1;
    for(int i = 0; i < MAX_JOBS; i++){
        if(!jobs_bg[i].en_uso){ slot = i; break; }
    }

    int id_asignado = -1;
    if(slot == -1){
        fprintf(stderr, "mishell: no hay espacio para jobs en background\n");
    }else{
        jobs_bg[slot].en_uso = 1;
        jobs_bg[slot].id = siguiente_job_id++;
        jobs_bg[slot].n_pids = n;
        jobs_bg[slot].n_vivos = n;
        jobs_bg[slot].terminado = 0;
        jobs_bg[slot].notificado = 0;

        for(int i = 0; i<n && i< MAX_PIPELINE; i++){
            jobs_bg[slot].pids[i] = pids[i];
        }
        strncpy(jobs_bg[slot].comando, texto_comando, sizeof(jobs_bg[slot].comando)-1);
        jobs_bg[slot].comando[sizeof(jobs_bg[slot].comando) -1 ] = '\0';
        id_asignado = jobs_bg[slot].id;
    }

    restaurar_mascara(&anterior);
    return id_asignado;

}


/*Manejador de sigchild*/
void manejador_sigchld(int senal){
    (void) senal;
    int errno_guardado = errno; //waitpid puede modificar errno
    int estado;
    pid_t pid;

    while((pid = waitpid(-1, &estado, WNOHANG)) > 0){
        for(int i = 0; i < MAX_JOBS; i++){
            if(!jobs_bg[i].en_uso) continue;
            for(int j = 0; j<jobs_bg[i].n_pids; j++){
                if(jobs_bg[i].pids[j] == pid){
                    jobs_bg[i].n_vivos--;
                    if(jobs_bg[i].n_vivos <= 0){
                        jobs_bg[i].terminado = 1;
                        hay_jobs_terminados = 1;
                    }
                }
            }
        }
    }

    errno = errno_guardado;

}

void revisar_jobs_terminados(void){
    if(!hay_jobs_terminados) return;

    sigset_t anterior;
    bloquear_sigchld(&anterior);
    hay_jobs_terminados = 0;

    for(int i = 0; i<MAX_JOBS; i++){
        if(jobs_bg[i].en_uso && jobs_bg[i].terminado && !jobs_bg[i].notificado){
            printf("[%d]+ Done\t%s\n", jobs_bg[i].id, jobs_bg[i].comando);
            jobs_bg[i].notificado = 1;
            jobs_bg[i].en_uso = 0; //libera el slot para un futuro job
        }
    }

    restaurar_mascara(&anterior);
}


/*Comando interno "jobs": lista los procesos en background que 
siguen activos*/
void listar_jobs(void){
    sigset_t anterior;
    bloquear_sigchld(&anterior);

    int alguno = 0;
    for(int i = 0; i <MAX_JOBS; i++){
        if(jobs_bg[i].en_uso && !jobs_bg[i].terminado){
            printf("[%d] Ejecutando\t%s\n", jobs_bg[i].id, jobs_bg->comando);
            alguno = 1;
        }
    }
    if(!alguno){
        printf("No hay jobs en background. \n");
    }

    restaurar_mascara(&anterior);
}


/*Configuracion de señales propias de la shell*/
void instalar_manejadores_shell(void){
    struct sigaction accion_ignorar;
    memset(&accion_ignorar, 0, sizeof(accion_ignorar));
    accion_ignorar.sa_handler = SIG_IGN;
    sigemptyset(&accion_ignorar.sa_mask);
    accion_ignorar.sa_flags = SA_RESTART;

    if(sigaction(SIGINT, &accion_ignorar, NULL) == -1) perror("sigaction SIGINT");
    if(sigaction(SIGQUIT, &accion_ignorar, NULL) == -1) perror("sigaction SIGQUIT");

    struct sigaction accion_sigchld;
    memset(&accion_sigchld, 0, sizeof(accion_sigchld));
    accion_sigchld.sa_handler = manejador_sigchld;
    sigemptyset(&accion_sigchld.sa_mask);
    accion_sigchld.sa_flags = SA_RESTART;


    if(sigaction(SIGCHLD, &accion_sigchld, NULL) == -1) perror("sigaction SIGCHLD");
}


/*Quita espacios finales y un eventual '&' de fin del linea, para guardar
en la tabla de jobs un texto limpio (usado por jobs/pmon y por el aviso "Done")*/
void limpiar_texto_comando(char *texto){
    int len = (int) strlen(texto);
    while(len > 0 && (texto[len -1] == ' ' || texto[len -1] == '\t')){
        texto[--len] = '\0';
    }

    if(len > 0 && texto[len -1] == '&'){
        texto[--len] = '\0';
        while(len > 0 && (texto[len - 1] == ' '  || texto[len -1] == '\t')){
            texto[--len] = '\0';
        }
    }
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
int ejecutar_pipeline(Comando *comandos, int n, int background, const char *texto_comando) {
    int (*pipes)[2] = NULL;
    if (n > 1) {
        pipes = malloc((n - 1) * sizeof(*pipes));
        for (int i = 0; i < n - 1; i++) {
            if (pipe(pipes[i]) == -1) { perror("pipe"); free(pipes); return -1; }
        }
    }

    pid_t *pids = malloc(n * sizeof(pid_t));
    pid_t pgid = 0;// pid del primer proceso del job; sirve de id de grupo


    /*Si el job va en background, se bloquea SIGCHLD antes de crear los
    procesos y se desbloquea recien despues de registrar el job en jobs_bg*/
    sigset_t mascara_previa;
    if(background) bloquear_sigchld(&mascara_previa);

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
             perror("fork");
             if(background) restaurar_mascara(&mascara_previa);
             free(pids);
             free(pipes);
             return -1;
             
        }

        if (pid == 0) {

            /*La shell ignora SIGINT/SIGQUIT, pero los procesos que ejecuta deben responder a ellos
            con su comportamiento por defecto. Se restaura antes del execvp para que el programa 
            reemplazado quede con la disposicion normal*/
            struct sigaction accion_dfl;
            memset(&accion_dfl, 0, sizeof(accion_dfl));
            accion_dfl.sa_handler = SIG_DFL;
            sigemptyset(&accion_dfl.sa_mask);
            sigaction(SIGINT, &accion_dfl, NULL);
            sigaction(SIGQUIT, &accion_dfl, NULL);

            if (background){

                /*Se aisla el job en su propio grupo de procesos para que
                un Ctrl + C hecho en el terminal no le llegue. El primer proceso del 
                pipeline se vuelve lider de un grupo nuevo; el resto se une a ese grupo*/

                if(i == 0) setpgid(0, 0);

                else      setpgid(0, pgid);
            }

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

        //proceso padre
        pids[i] = pid;

        if(background) {

            if(i == 0) { pgid = pid; setpgid(pid, pid);}
            else       {setpgid(pid, pgid);}
        }
    }

    if (n > 1) {
        for (int j = 0; j < n - 1; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
    }

    if(background){
        int id = registrar_job(pids, n, texto_comando);


        if(id !=-1){
            printf("[%d] %d\n", id, pids[0]);
            fflush(stdout);
        }

        restaurar_mascara(&mascara_previa);
    } else {

        int estado;
        for (int i = 0; i < n; i++) waitpid(pids[i], &estado, 0);
    }

    free(pipes);
    free(pids);
    return 0;
}


int main(void){
    char linea[4096]; //lugar en el que se guardará la línea que ingrese el usuario


    /*la shell debe ignorar SIGINT/SIGQUIT y recolectar hijos en background
    mediante SIGCHLD*/
    instalar_manejadores_shell();


    //el ciclo a continuación se repetirá continuamente, debido a que es algo 
    //que debe realizar la shell en todo momento que esté activa.
    while(1){

        revisar_jobs_terminados();


        mostrar_prompt();
        
        if(fgets(linea, sizeof(linea), stdin) == NULL){


            if(ferror(stdin) && errno == EINTR){
                clearerr(stdin);
                continue;
            }
            printf("\n");
            break;
        }

        linea[strcspn(linea, "\n")] = '\0';


        char linea_para_mostrar[4096];
        strncpy(linea_para_mostrar, linea, sizeof(linea_para_mostrar) -1);
        linea_para_mostrar[sizeof(linea_para_mostrar) - 1] = '\0';

        char **argv = separar_tokens(linea); //separa la linea en tokens

        if(argv == NULL){
            return EXIT_FAILURE;
        }

        if(argv[0] == NULL){
            free(argv);
            continue;
        }

        int n_tokens = 0;
        while(argv[n_tokens] != NULL) n_tokens++;

        int background = 0;
        if(n_tokens > 0 && strcmp(argv[n_tokens -1], "&") == 0){
            background = 1;
            argv[n_tokens -1] = NULL;

            if(argv[0] == NULL){ //la linea era solo "&"
                free(argv);
                continue;
            }
        }

        limpiar_texto_comando(linea_para_mostrar);


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
        if(strcmp(argv[0], "jobs") == 0){
            listar_jobs();
            free(argv);
            continue;
        }

        //pmon == depende de la seccion 3

        int n_comandos;
        Comando *comandos = construir_pipeline(argv, &n_comandos);
        free(argv);

        if (comandos != NULL) {
            ejecutar_pipeline(comandos, n_comandos, background, linea_para_mostrar);
            for (int i = 0; i < n_comandos; i++) free(comandos[i].argv);
            free(comandos);
        }

    }
    return 0;
}
#include "child.h"
#include "display.h"
#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

Circuit circuit;
F1_Car *car;


int car_names[NUMBER_OF_CARS] = {44, 77, 5, 7, 3, 33, 11, 31, 18, 35,
                                 27, 55, 10, 28, 8, 20, 2, 14, 9, 16};

int qualified_cars[15], race_ranking[20], last_cars_of_Q1[15], last_cars_of_Q2[10];

void print_usage() {
    printf("%s", "Usage: ./prog --day [dayName] --step [stepName]\n");
    printf("%s", "Usage: For race you can specify the lap length, by default it's 7km !\n");
    printf("%s", "Usage: ./prog --day [dayName] --step [stepName] --length [number]\n");
    printf("%s", "Use the --help command for more information. \n");
    exit(EXIT_FAILURE);
}

void help() {
    printf("\n%s\n\n", "These are some commands used to run this program.");
    printf("%s\n", "For P sessions : \t There are run on fridays, use the --day command.");
    printf("%s\n", "\t\t\t followed by a day name and which step needs to be runned for the P sessions.");
    printf("%s\n\n", "\t\t\t --day fri --step P3 for instance.");
    printf("%s\n", "For Q sessions : \t There are run on saturdays, use the --day command.");
    printf("%s\n", "\t\t\t followed by a day name and which step needs to be runned for the Q sessions.");
    printf("%s\n\n", "\t\t\t --day sat --step Q3 for instance.");
    printf("%s\n", "For the RACE session : \t It's run on sundays, use the --day command.");
    printf("%s\n", "\t\t\t followed by a day name. Here you can specify the race's lap length.");
    printf("%s\n", "\t\t\t by default it's 7km, the --length command is optional.");
    printf("%s\n\n", "\t\t\t --day sun --step RACE --length 10 for instance. ");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {

    signal(SIGINT, return_cursor);

    circuit.lap_km = 7;
    circuit.race_km = 305;

    int user_km = 0;
    char day_name[5], step_name[5];

    static struct option long_options[] = {{"day",    required_argument, NULL, 'd'},
                                           {"step",   required_argument, NULL, 's'},
                                           {"length", required_argument, NULL, 'l'},
                                           {"help",   no_argument, 0,          'h'},
                                           {NULL, 0,                     NULL, 0}};

    char opt;
    while ((opt = getopt_long(argc, argv, "hd:s:l:", long_options, NULL)) != EOF) {
        switch (opt) {
            case 'h':
                help();
                break;
            case 'd':
                strcpy(day_name, optarg);
                break;
            case 's':
                strcpy(step_name, optarg);
                break;
            case 'l':
                user_km = atoi(optarg);
                break;
            default:
                print_usage();
        }
    }

    if (!strcmp(day_name, "fri")) {
        if (!strcmp(step_name, "P1")) {
            circuit = (Circuit) {.number_of_cars = 20, .step_name = "P1", .step_total_time = minutes_to_ms(90)};
        } else if (!strcmp(step_name, "P2")) {
            circuit = (Circuit) {.number_of_cars = 20, .step_name = "P2", .step_total_time = minutes_to_ms(90)};
        } else {
            print_usage();
        }
    } else if (!strcmp(day_name, "sat")) {
        if (!strcmp(step_name, "P3")) {
            circuit = (Circuit) {.number_of_cars = 20, .step_name = "P3", .step_total_time = minutes_to_ms(60)};
        } else if (!strcmp(step_name, "Q1")) {
            circuit = (Circuit) {.number_of_cars = 20, .step_name = "Q1", .step_total_time = minutes_to_ms(18)};
        } else if (!strcmp(step_name, "Q2")) {
            circuit = (Circuit) {.number_of_cars = 15, .step_name = "Q2", .step_total_time = minutes_to_ms(15)};
            read_files(qualified_cars, race_ranking, last_cars_of_Q1, last_cars_of_Q2, "Q1", 15);
        } else if (!strcmp(step_name, "Q3")) {
            circuit = (Circuit) {.number_of_cars = 10, .step_name = "Q3", .step_total_time = minutes_to_ms(12)};
            read_files(qualified_cars, race_ranking, last_cars_of_Q1, last_cars_of_Q2, "Q2", 10);
        } else {
            print_usage();
        }
    } else if (!strcmp(day_name, "sun")) {
        if (!strcmp(step_name, "RACE")) {
            circuit.number_of_cars = 20;
            circuit.step_name = "RACE";
            circuit.step_total_time = minutes_to_ms(120);
            read_files(qualified_cars, race_ranking, last_cars_of_Q1, last_cars_of_Q2, "Q3", 10);
            read_eliminated_cars("lastQ2", race_ranking);
            read_eliminated_cars("lastQ1", race_ranking);
            read_eliminated_cars("Q3", race_ranking);

            if (user_km == 0) {
                circuit.number_of_laps = circuit.race_km / circuit.lap_km;
            } else if (user_km > 0) {
                circuit.number_of_laps = circuit.race_km / user_km;
            } else {
                print_usage();
            }
        } else {
            print_usage();
        }
    } else {
        print_usage();
    }

    !strcmp(circuit.step_name, "Q2") ? save_eliminated_cars("lastQ1", last_cars_of_Q1)
                                     : !strcmp(circuit.step_name, "Q3") ? save_eliminated_cars("lastQ2",
                                                                                               last_cars_of_Q2)
                                                                        : NULL;

    int struct_shm_id = shmget(IPC_PRIVATE, sizeof(F1_Car) * circuit.number_of_cars, 0600 | IPC_CREAT);
    if (struct_shm_id == -1) {
        perror("shmget failed !");
        exit(1);
    }

    car = shmat(struct_shm_id, NULL, 0);
    if (car == (void *) (-1)) {
        perror("shmat failed !");
        exit(1);
    }

    int sem_shm_id = shmget(IPC_PRIVATE, sizeof(sem_t), 0600 | IPC_CREAT);
    if (sem_shm_id == -1) {
        perror("shmget failed !");
        exit(1);
    }
    sem_t *sem = shmat(sem_shm_id, NULL, 0);
    if (sem == (void *) (-1)) {
        perror("shmat failed !");
        exit(EXIT_FAILURE);
    }

    sem_init(sem, 1, 1);

    int i;
    pid_t pid = 0;
    for (i = 0; i < circuit.number_of_cars; i++) {
        pid = fork();
        if (pid == 0)
            break;
    }

    switch (pid) {
        case -1:
            fprintf(stderr, "fork failed !");
            exit(-1);
        case 0:
            (!strcmp(circuit.step_name, "Q2") || !strcmp(circuit.step_name, "Q3")) ?
            child(sem, &car[i], &qualified_cars[i]) : !strcmp(circuit.step_name, "RACE") ?
                                                      child(sem, &car[i], &race_ranking[i]) :
                                                      child(sem, &car[i], &car_names[i]);

            exit(0);
        default:
            display(sem, car);
            for (int j = 0; j < circuit.number_of_cars; j++) {
                wait(NULL);
            }
    }
    shmdt(car);
    shmctl(struct_shm_id, IPC_RMID, NULL);

    sem_destroy(sem);
    shmdt(sem);
    shmctl(sem_shm_id, IPC_RMID, NULL);
    exit(EXIT_SUCCESS);

}
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

// Estrutura para representar um objeto no ecossistema
typedef struct
{
    char type;
    int x, y;
    int lastMeal;
    int generationsSinceReproduction;
    int markedForDeath; // Novo campo para marcar objetos para remoção
} Object;

// Função para ler o ecossistema inicial do arquivo de entrada
void readInitialEcosystem(FILE *file, Object **ecosystem, int *numObjects)
{
    fscanf(file, "%*d %*d %*d %*d %*d %*d %d", numObjects);

    *ecosystem = (Object *)malloc((*numObjects) * sizeof(Object));

#pragma omp parallel for
    for (int i = 0; i < *numObjects; i++)
    {
        fscanf(file, " %c %d %d", &((*ecosystem)[i].type), &((*ecosystem)[i].x), &((*ecosystem)[i].y));
        (*ecosystem)[i].lastMeal = 0;
        (*ecosystem)[i].generationsSinceReproduction = 0;
        (*ecosystem)[i].markedForDeath = 0;
    }
}

// Função para verificar se uma célula está ocupada por uma raposa
int isCellOccupiedByFox(Object *ecosystem, int x, int y, int numObjects)
{
    for (int i = 0; i < numObjects; i++)
    {
        if (ecosystem[i].type == 'R' && ecosystem[i].x == x && ecosystem[i].y == y)
        {
            return 1; // A célula está ocupada por uma raposa
        }
    }
    return 0; // A célula não está ocupada
}

// Função para verificar se uma célula está ocupada por um coelho
int isCellOccupiedByCoelho(Object *ecosystem, int x, int y, int numObjects)
{
    for (int i = 0; i < numObjects; i++)
    {
        if (ecosystem[i].type == 'C' && ecosystem[i].x == x && ecosystem[i].y == y)
        {
            return 1; // A célula está ocupada por um coelho
        }
    }
    return 0; // A célula não está ocupada
}

// Função para verificar se uma célula está ocupada por uma rocha
int isCellOccupiedByRock(Object *ecosystem, int x, int y, int numObjects)
{
    for (int i = 0; i < numObjects; i++)
    {
        if (ecosystem[i].type == 'O' && ecosystem[i].x == x && ecosystem[i].y == y)
        {
            return 1; // A célula está ocupada por uma rocha
        }
    }
    return 0; // A célula não está ocupada
}

// Função para verificar se uma célula está ocupada
int isCellOccupied(Object *ecosystem, int x, int y, int numObjects)
{
    for (int i = 0; i < numObjects; i++)
    {
        if (ecosystem[i].x == x && ecosystem[i].y == y)
        {
            // A célula está ocupada
            return 1;
        }
    }
    // A célula está vazia
    return 0;
}

// Função para criar um novo objeto no ecossistema
void createNewObject(Object *ecosystem, char objectType, int x, int y, int *numObjects)
{
    if (objectType == 'F' || objectType == 'C' || objectType == 'O')
    {
#pragma omp critical
        {
            ecosystem[*numObjects].type = objectType;
            ecosystem[*numObjects].x = x;
            ecosystem[*numObjects].y = y;
            (*numObjects)++;
        }
    }
}

void removeObject(Object *ecosystem, int index, int *numObjects)
{
#pragma omp critical
    {
        // Move o último objeto para a posição do objeto que será removido
        ecosystem[index] = ecosystem[*numObjects - 1];

        // Decrementa o número total de objetos
        (*numObjects)--;
    }
}

// Função para encontrar o alvo (coelho) para a raposa comer
int findTarget(Object *ecosystem, int foxIndex, int numObjects, int GEN_FOX_HUNGER, int GEN_PROC_FOXES)
{
    int targetIndex = -1;
    int oldestReproductionAge = GEN_PROC_FOXES;

#pragma omp parallel for
    for (int i = 0; i < numObjects; i++)
    {
        if (ecosystem[i].type == 'C' && !ecosystem[i].markedForDeath)
        {
            // Coelho encontrado
            if (ecosystem[i].generationsSinceReproduction < oldestReproductionAge)
            {
                // Atualiza o alvo com base na idade de reprodução mais antiga
                targetIndex = i;
                oldestReproductionAge = ecosystem[i].generationsSinceReproduction;
            }
        }
    }

    // Marca o coelho para ser removido
    if (targetIndex != -1)
    {
        ecosystem[targetIndex].markedForDeath = 1;
    }

    return targetIndex;
}

// Função para remover um coelho marcado para a morte
void removeMarkedRabbits(Object *ecosystem, int *numObjects)
{
    int i = 0;

    while (i < *numObjects)
    {
        if (ecosystem[i].markedForDeath)
        {
            // Remove o coelho marcado para a morte
            removeObject(ecosystem, i, numObjects);
        }
        else
        {
            i++;
        }
    }
}

// Função para comer um coelho e remover coelhos marcados para a morte
void eatRabbit(Object *ecosystem, int foxIndex, int rabbitIndex, int *numObjects)
{
    // Atualiza as coordenadas da raposa
    ecosystem[foxIndex].x = ecosystem[rabbitIndex].x;
    ecosystem[foxIndex].y = ecosystem[rabbitIndex].y;

    // Zera a contagem de gerações desde a última refeição
    ecosystem[foxIndex].generationsSinceReproduction = 0;

    // Remove o coelho marcado para a morte
    ecosystem[rabbitIndex].markedForDeath = 1;

    // Remove outros coelhos marcados para a morte
    removeMarkedRabbits(ecosystem, numObjects);
}

// Função para mover um objeto em uma direção aleatória
void moveObject(Object *ecosystem, int index, int numObjects, int L, int R, int *x, int *y, int direction)
{
#pragma omp critical
    {
        switch (direction)
        {
        case 0: // Norte
            *x = (*x - 1 + L) % L;
            break;
        case 1: // Leste
            *y = (*y + 1) % R;
            break;
        case 2: // Sul
            *x = (*x + 1) % L;
            break;
        case 3: // Oeste
            *y = (*y - 1 + R) % R;
            break;
        }
    }
}

void moveRabbit(Object *ecosystem, int index, int numObjects, int L, int R, int GEN_PROC_RABBITS, int GEN_PROC_FOXES)
{
    int x, y;

    // Loop até encontrar uma célula livre ou atingir o limite de tentativas
    for (int attempt = 0; attempt < 4; attempt++)
    {
        int direction = rand() % 4;
        x = ecosystem[index].x;
        y = ecosystem[index].y;

        moveObject(ecosystem, index, numObjects, L, R, &x, &y, direction);

        if (!isCellOccupied(ecosystem, x, y, numObjects))
        {
            ecosystem[index].x = x;
            ecosystem[index].y = y;

            if (ecosystem[index].generationsSinceReproduction == GEN_PROC_RABBITS)
            {
                ecosystem[index].generationsSinceReproduction = 0;
                createNewObject(ecosystem, 'C', x, y, &numObjects);
            }
            else
            {
                ecosystem[index].generationsSinceReproduction++;
            }

            return;
        }
    }

    // Se todas as tentativas falharem, marca o coelho para remoção
    ecosystem[index].markedForDeath = 1;
}

void moveFox(Object *ecosystem, int index, int numObjects, int L, int R, int GEN_PROC_FOXES, int GEN_FOX_HUNGER)
{
    int x, y;

    // Loop até encontrar uma célula livre ou a fome da raposa atingir o limite
    for (int attempt = 0; attempt < 4; attempt++)
    {
        int direction = rand() % 4;
        x = ecosystem[index].x;
        y = ecosystem[index].y;

        moveObject(ecosystem, index, numObjects, L, R, &x, &y, direction);

        if (!isCellOccupied(ecosystem, x, y, numObjects))
        {
            // A célula está livre; move a raposa
            ecosystem[index].x = x;
            ecosystem[index].y = y;

            if (ecosystem[index].generationsSinceReproduction == GEN_PROC_FOXES)
            {
                ecosystem[index].generationsSinceReproduction = 0;
                createNewObject(ecosystem, 'R', x, y, &numObjects);
            }
            else
            {
                ecosystem[index].generationsSinceReproduction++;
            }

            return; // Sai do loop pois a movimentação foi realizada com sucesso
        }
        else if (ecosystem[index].generationsSinceReproduction >= GEN_FOX_HUNGER)
        {
            // A fome da raposa atingiu o limite; a raposa morre
            ecosystem[index].markedForDeath = 1;
            return;
        }
    }

    // Se todas as tentativas falharem, marca a raposa para remoção
    ecosystem[index].markedForDeath = 1;
}

void simulateGeneration(Object *ecosystem, int *numObjects, int L, int R, int GEN_PROC_RABBITS, int GEN_PROC_FOXES, int GEN_FOX_HUNGER)
{
    // Número total de objetos no início da geração
    int initialNumObjects = *numObjects;

    // Loop para cada objeto no ecossistema
    for (int i = 0; i < initialNumObjects; i++)
    {
        // Verifica o tipo do objeto
        if (ecosystem[i].type == 'C')
        {
            moveRabbit(ecosystem, i, *numObjects, L, R, GEN_PROC_RABBITS, GEN_PROC_FOXES);
        }
        else if (ecosystem[i].type == 'R')
        {
            moveFox(ecosystem, i, *numObjects, L, R, GEN_PROC_FOXES, GEN_FOX_HUNGER);
        }
    }

    // Atualizar o número total de objetos após todas as movimentações e remoções
    *numObjects = initialNumObjects;

    // Remover objetos marcados para a morte após todos os movimentos de coelhos
    removeMarkedRabbits(ecosystem, numObjects);
}

void writeFinalEcosystem(FILE *outputFile, Object *ecosystem, int numObjects, int GEN_PROC_RABBITS, int GEN_PROC_FOXES, int GEN_FOX_HUNGER, int N_GEN, int L, int R)
{
    fprintf(outputFile, "%d %d %d %d %d %d %d\n", GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOX_HUNGER, (N_GEN - N_GEN), L, R, numObjects);

    for (int i = 0; i < numObjects; i++)
    {
        fprintf(outputFile, "%c %d %d\n", ecosystem[i].type, ecosystem[i].x, ecosystem[i].y);
    }
}

int main(int argc, char *argv[]) {
    double time_spent = 0.0;
    FILE *inputFile, *outputFile;
    int GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOX_HUNGER, N_GEN, L, R, numObjects;
    Object *ecosystem;
    int numProcesses, processId;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);
    MPI_Comm_rank(MPI_COMM_WORLD, &processId);

    if (processId == 0) {
        clock_t begin = clock();
        // Abrir o arquivo de entrada
        inputFile = fopen("input.txt", "r");
        if (inputFile == NULL) {
            fprintf(stderr, "Erro ao abrir o arquivo de entrada.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Ler configurações iniciais do ecossistema
        fscanf(inputFile, "%d %d %d %d %d %d %d", &GEN_PROC_RABBITS, &GEN_PROC_FOXES, &GEN_FOX_HUNGER, &N_GEN, &L, &R, &numObjects);
        readInitialEcosystem(inputFile, &ecosystem, &numObjects);

        // Fechar o arquivo de entrada
        fclose(inputFile);

        for (int i = 0; i < N_GEN; i++) {
            for (int j = 0; j < numObjects; j++) {
                // Verifica o tipo do objeto
                if (ecosystem[j].type == 'C') {
                    moveRabbit(ecosystem, j, numObjects, L, R, GEN_PROC_RABBITS, GEN_PROC_FOXES);
                } else if (ecosystem[j].type == 'R') {
                    moveFox(ecosystem, j, numObjects, L, R, GEN_PROC_FOXES, GEN_FOX_HUNGER);
                }
            }

            for (int j = 0; j < numObjects; j++) {
                if (ecosystem[j].markedForDeath) {
                    // Remove o coelho ou raposa marcado para a morte
                    removeObject(ecosystem, j, &numObjects);
                }
            }
        }

        // Abrir o arquivo de saída
        outputFile = fopen("output.txt", "w");
        if (outputFile == NULL) {
            fprintf(stderr, "Erro ao abrir o arquivo de saída.\n");
            free(ecosystem);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Escrever estado final do ecossistema no arquivo de saída
        writeFinalEcosystem(outputFile, ecosystem, numObjects, GEN_PROC_RABBITS, GEN_PROC_FOXES, GEN_FOX_HUNGER, N_GEN, L, R);
        clock_t end = clock();
        time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
        printf("Tempo de execução: %f segundos\n", time_spent);

        // Fechar o arquivo de saída
        fclose(outputFile);

        // Liberar memória alocada para o ecossistema
        free(ecosystem);
    }

    MPI_Finalize();

    return 0;
}


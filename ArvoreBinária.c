#include <stdio.h>
#include <math.h>-
#include <stdlib.h>

//Jefferson dami�o da silva lima
//Mathews Pereira Ara�jo
//Marcelo viana da silva
//vitor de jesus ferreira mota

typedef struct no{
    int valor;
    struct no *prox;
}No;

typedef struct {
   No *inicio;
   int tam;
}Lista;

//Criar a lista
void criarLista(Lista *lista) {
   lista->inicio = NULL;
   lista->tam = 0;
}

void inserirOrdenado(Lista *lista, int num) {
   No *aux, *novo = malloc(sizeof(No));

   if(novo) {
     novo->valor = num;
     if(lista->inicio == NULL) {
         novo->prox = NULL;
         lista->inicio = novo;
     } else if(novo->valor < lista->inicio->valor) {
         novo->prox = lista->inicio;
         lista->inicio = novo;
     }else {
         aux = lista->inicio;
         while(aux->prox && novo->valor > aux->prox->valor) {
            aux = aux->prox;
         }
         novo->prox = aux->prox;
         aux->prox = novo;
     }
     lista->tam++;
   }
   else {
      printf("Erro ao alocar mem�ria");
   }
}

// Remover um elemento da lista
No* remover(Lista *lista, int num) {
   No *aux, *remover = NULL;

   if(lista->inicio) {
      if(lista->inicio->valor == num) {
         remover = lista->inicio;
         lista->inicio = remover->prox;
         lista->tam--;
      } else {
         aux = lista->inicio;
         while(aux->prox && aux->prox->valor != num) {
            aux = aux->prox;
         }
         if(aux->prox) {
            remover = aux->prox;
            aux->prox = remover->prox;
            lista->tam--;
         }
      }
   }

   return remover;
}

No* buscar(Lista *lista, int num) {
   No *aux, *no = NULL;
   aux = lista->inicio;

   while(aux && aux->valor != num) {
      aux = aux->prox;
   }
   if(aux) {
      no = aux;
   }

   return no;
}

void imprimir(Lista lista) {
   No *no = lista.inicio;

   printf("\n\tLista tam %d: ", lista.tam);
   while(no) {
      printf("%d ", no->valor);
      no = no->prox;
   }
   printf("\n\n");
}

int main()
{
    
    int opcao, valor, anterior;
    Lista lista;
    No *removido;
    No *buscado;

    criarLista(&lista);

    do{
      //printf("\n\t0 - Sair\n\t1 - Inserir no in�cio\n\t2 - Inserir no fim\n\t3 - Inserir no meio\n\t4 - Inserir ordenado\n\t5 - Remover\n\t6 - Imprimir\n\t7 - Buscar\n");
      printf("\n\t0 - Sair \n\t1 - Inserir ordenado\n\t2 - Remover\n\t3 - Imprimir\n\t4 - Buscar\n");
      scanf("%d", &opcao);

      switch(opcao) {
         case 1:
            printf("Digite um valor: ");
            scanf("%d", &valor);
            inserirOrdenado(&lista, valor);
            break;
         case 2:
            printf("Digite um valor a ser removido: ");
            scanf("%d", &valor);
            removido = remover(&lista, valor);
            if(removido) {
               printf("Elemento removido: %d\n", removido->valor);
               free(removido);
            } else {
               printf("Nenhuma elemento foi removido");
            }
            break;
         case 3:
            imprimir(lista);
            break;
         case 4:
            printf("Digite um valor a ser buscado: ");
            scanf("%d", &valor);
            buscado = buscar(&lista, valor);
            if(buscado)
               printf("Valor encontrado: %d\n", buscado->valor);
            else
               printf("Valor n�o encontrado");
            break;
         default:
            if(opcao != 0) {
               printf("Op��o inv�lida!\n");
            }
      }
    }while(opcao != 0);

    return 0;
}

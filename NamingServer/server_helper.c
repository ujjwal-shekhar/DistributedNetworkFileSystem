# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdbool.h>

# define NUM_CHARS 256
# define MAX_STORAGE_SERVERS 20

bool inStorageServer[MAX_STORAGE_SERVERS];
// The code for tries was referred to from Jacob Sorber's code with changes fine-tuned to our use case

typedef struct trienode{
    struct trienode *children[NUM_CHARS];
    bool inStorageServer[MAX_STORAGE_SERVERS];
    bool isEndOfWord;
} trienode;

trienode* createnode(){
    trienode* node = (trienode*)malloc(sizeof(trienode));

    for(int i=0; i<NUM_CHARS; i++){
        node->children[i] = NULL;
    }

    for(int i=0; i<MAX_STORAGE_SERVERS; i++){
        node->inStorageServer[i] = false;
    }

    node->isEndOfWord = false;
}

void trieinsert(trienode** root, char* signedtext, int serverID){
    if(*root == NULL){
        *root = createnode();
    }

    unsigned char* text = (unsigned char*)signedtext;     // just in case we don't get negative indexes
    trienode* curr = *root;
    int length = strlen(signedtext);

    for(int i=0 ; i<length ; i++){
        if(curr->children[text[i]] == NULL){
            // create a node
            curr->children[text[i]] = createnode();
        }

        curr = curr->children[text[i]];
    }

    curr->isEndOfWord = true;
    curr->inStorageServer[serverID] = true;
}

bool search_trie(trienode* root, char* signedtext){
    if(root == NULL){
        return false;
    }

    unsigned char* text = (unsigned char*)signedtext;
    trienode* curr = root;
    int length = strlen(signedtext);

    for(int i=0 ; i<length ; i++){
        if(curr->children[text[i]] == NULL){
            return false;
        }

        curr = curr->children[text[i]];
    }

    // mark the servers in which this file is present
    for(int i=0; i<MAX_STORAGE_SERVERS; i++){
        if(curr->inStorageServer[i]){
            inStorageServer[i] = true;
        }
    }

    return curr->isEndOfWord;
}

void print_trie(trienode* root){
    if(root == NULL){
        return;
    }

    for(int i=0; i<NUM_CHARS; i++){
        if(root->children[i] != NULL){
            printf("%c", i);
            print_trie(root->children[i]);
        }
    }

    if(root->isEndOfWord){
        printf("\n");
    }
}

void reset_inStorageServer(){
    for(int i=0; i<MAX_STORAGE_SERVERS; i++){
        inStorageServer[i] = false;
    }
}

void print_inStorageServer(){
    for(int i=0; i<MAX_STORAGE_SERVERS; i++){
        if(inStorageServer[i]){
            printf("%d ", i);
        }
    }
    printf("\n");
}

int main(){

    trienode* root = NULL;

    trieinsert(&root, "hello", 1);
    trieinsert(&root, "hell", 2);
    trieinsert(&root, "helloworld", 3);
    trieinsert(&root, "cat", 4);
    trieinsert(&root, "dog", 5);
    trieinsert(&root, "cattle", 6);
    trieinsert(&root, "cattle", 7);

    print_trie(root);

    // search for sample words
    
}

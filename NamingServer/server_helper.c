# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdbool.h>

# define NUM_CHARS 256
# define MAX_WORD_LEN 100

// The code for tries was referred to from Jacob Sorber's code with changes fine-tuned to our use case

typedef struct trienode{
    struct trienode *children[NUM_CHARS];
    int storage_server;
    bool isFile;
    bool isEndOfWord;
} trienode;

trienode* createnode() {
    trienode* node = (trienode*) malloc(sizeof(trienode));

    for(int i=0; i<NUM_CHARS; i++){
        node->children[i] = NULL;
    }
    node->storage_server = -1;
    node->isFile = false;
    node->isEndOfWord = false;
    return node;
}

void trieinsert (trienode** root, char* signedtext, int serverID) {
    if (*root == NULL) {
        *root = createnode();
    }

    unsigned char* text = (unsigned char*) signedtext;     // just in case we don't get negative indexes
    trienode* curr = *root;
    int length = strlen(signedtext);

    for (int i = 0 ; i < length ; i++) {
        if(curr->children[text[i]] == NULL){
            // create a node
            curr->children[text[i]] = createnode();
        }

        curr = curr->children[text[i]];
        if (text[i]=='/') {
            curr->isFile = false;
            curr->storage_server = serverID;
            curr->isEndOfWord = true;
        }
    }

    if (text[length-1]!='/') {
        curr->isFile = true;
        curr->isEndOfWord = true;
        curr->storage_server = serverID;
    }
    
}


int search_trie (trienode* root, char* signedtext) {
    if(root == NULL){
        return -1;
    }

    // printf("Searching for the path : %s", signedtext);

    unsigned char* text = (unsigned char*)signedtext;
    trienode* curr = root;
    int length = strlen(signedtext);
    printf("\nSearching:");
    for(int i=0 ; i<length ; i++){
        if(curr->children[text[i]] == NULL){
            return -1;
        }
        printf("%c", text[i]);
        curr = curr->children[text[i]];

        if(curr->isEndOfWord && curr ->storage_server == -1){
            // the path/directory has been deleted
            return -1;
        }
    }
    printf("\n");

    return ((curr->isEndOfWord) ? (curr->storage_server) : (-1)); // -1 marks path nowhere
}

void delete_from_trie(trienode** root, char* signedtext){
    if(*root == NULL){
        return;
    }

    if(search_trie(*root, signedtext) == -1){
        return;
    }

    unsigned char* text = (unsigned char*)signedtext;
    trienode* curr = *root;
    int length = strlen(signedtext);

    for(int i=0 ; i<length ; i++){
        if(curr->children[text[i]] == NULL){
            return;
        }

        curr = curr->children[text[i]];
    }
    printf("Marking -1 for char %c\n", text[length-1]);
    curr-> storage_server = -1;
}

int main(){

    trienode* root = NULL;

    trieinsert(&root, "/dir1/dir2/file1.txt", 1);
    printf("Searching for '/dir1/dir2/file1.txt' : %d\n", search_trie(root, "/dir1/dir2/file1.txt"));

    delete_from_trie(&root,"/dir1/");
    printf("Searching for '/dir1/dir2/' : %d\n", search_trie(root, "/dir1/dir2/file1.txt"));
    // trieinsert(&root, "/dir1/dir2/file2.txt", 1);
    // trieinsert(&root, "/dir1/dir2/file3.txt", 3);
    // trieinsert(&root, "/dir1/dir2/file1.txt", 2);
    // trieinsert(&root, "/dir1/dir2/file1.txt", 3);
    // trieinsert(&root, "/dir1/dir2/", 3);
    // trieinsert(&root, "/dir3/dir2/file1.txt", 4);
    // trieinsert(&root, "/dir3/dir2/file1.txt", 5);
    // trieinsert(&root, "/dir4/file1.txt", 6);
    // trieinsert(&root, "/file1.txt", 7);

    // // print_trie(root);

    // // search for sample words

    // printf("Searching for '/dir1/dir2/fil' : %d\n", search_trie(root, "/dir1/dir2/fil"));

    // printf("Searching for '/file1.txt' : %d\n", search_trie(root, "/file1.txt"));
    
}

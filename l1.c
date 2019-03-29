#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#define DEBUG 0

#define PRINTLINE 0

#define LINELENGTH 128

#define ADDRESS_SIZE 32

char *printmessage;

typedef struct Cache_* Cache;
typedef struct Cacheline_* Cacheline;
typedef struct Way_* Way;

Cache createCache(int cache_size, int Cacheline_size, int write_policy, int argNumWays, int argNumCachelines);

void destroyCache(Cache cache, int argNumCachelines, int argNumWays);

int readFromCache(Cache cache, char* address, int argNumWays, int bitsTag, int bitsIndex);

int writeToCache(Cache cache, char* address, int argNumWays, int bitsTag, int bitsIndex);

int invalidateInstruction(Cache cache, char* address, int argNumWays, int bitsTag, int bitsIndex);

int dataRequest(Cache cache, char* address, int argNumWays, int bitsTag, int bitsIndex);

void resetCache(Cache cache, int argNumCachelines, int argNumWays);

void printCache(Cache cache, int argNumCachelines, int argNumWays);

void printAllCache(Cache cache);

struct node
{
    int data;
    struct node *next;
};

struct Cacheline_ {
    int mesi;
    int firstWrite;
    char* tag;
    struct node* list;
};

struct Way_ {
    int waynum;
    Cacheline* cachelines;
};

struct Cache_ {
    int hits;
    int misses;
    int reads;
    int writes;
    int evictions;
    int cache_size;
    int Cacheline_size;
    int write_policy;
    int associativity;
    Way* ways;
};

void UpdateLRU(struct node *head, int value);
void deleteNode(struct node *head, int value);
void push(struct node **head_ref, int new_data);
void printList(struct node *head);
int findLeast(struct node* head);
int findLast(struct node* head);
char findMESI(Cacheline Cacheline);
void printLRU(struct node *head, int i);
int findLRU(struct node *head, int i);

// global variables for program arguments

int argNumCachelinesData = 128;
int argNumCachelinesInstrction = 128;
int argNumWaysData = 4;
int argNumWaysInstrcution = 2;
int argCachelineSize = 64;
//int argCacheSize = 0;

// global variables for cache bits

int bitsTag = 0;
int bitsIndex = 0;
int bitsByteSelect = 0;

/* htoi
 *
 * Converts hexidecimal memory locations to unsigned integers.
 * No real error checking is performed. This function will skip
 * over any non recognized characters.
 */
 
unsigned int htoi(const char str[])
{
    /* Local Variables */
    unsigned int result;
    int i;

    i = 0;
    result = 0;
    
    while(str[i] != '\0')
    {
        result = result * 16;
        if(str[i] >= '0' && str[i] <= '9')
        {
            result = result + (str[i] - '0');
        }
        else if(tolower(str[i]) >= 'a' && tolower(str[i]) <= 'f')
        {
            result = result + (tolower(str[i]) - 'a') + 10;
        }
        i++;
    }

    return result;
}

/* getBinary
 *
 * Converts an unsigned integer into a string containing it's
 * 32 bit long binary representation.
 *
 */
 
char *getBinary(unsigned int num)
{
    char* bstring;
    int i;
    
    /* Calculate the Binary String */
    
    bstring = (char*) malloc(sizeof(char) * 33);
    
    bstring[32] = '\0';
    
    for( i = 0; i < 32; i++ )
    {
        bstring[32 - 1 - i] = (num == ((1 << i) | num)) ? '1' : '0';
    }
    
    return bstring;
}

/* formatBinary
 *
 * Converts a 32 bit long binary string to a formatted version
 * for easier parsing. The format is determined by the bitsTag, bitsIndex,
 * and bitsByteSelect variables.
 *
 * Ex. Format:
 *  -----------------------------------------------------
 * | Tag: 18 bits | Index: 12 bits | Byte Select: 4 bits |
 *  -----------------------------------------------------
 *
 * Ex. Result:
 * 000000000010001110 101111011111 00
 *
*/
char *formatBinary(char *bstring, int bitsTag, int bitsIndex )
{
    char *formatted;
    int i;
    
    /* Format for Output */
    
    formatted = (char *) malloc(sizeof(char) * 35);
    
    formatted[34] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        formatted[i] = bstring[i];
    }
    
    formatted[bitsTag] = ' ';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        formatted[i] = bstring[i - 1];
    }
    
    formatted[bitsIndex + bitsTag + 1] = ' ';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsByteSelect + bitsIndex + bitsTag + 2; i++)
    {
        formatted[i] = bstring[i - 2];
    }

    return formatted;
}

/* btoi
 *
 * Converts a binary string to an integer. Returns 0 on error.
 *
 * src: http://www.daniweb.com/software-development/c/code/216372
 *
 */

int btoi(char *bin)
{
    int  b, k, m, n;
    int  len, sum;

    sum = 0;
    len = strlen(bin) - 1;

    for(k = 0; k <= len; k++)
    {
        n = (bin[k] - '0'); 
        if ((n > 1) || (n < 0))
        {
            return 0;
        }
        for(b = 1, m = len; m > k; m--)
        {
            b *= 2;
        }
        sum = sum + n * b;
    }
    return(sum);
}

int main(int argc, char **argv) {

    /* Local Variables */
    int write_policy = 1, counter, i, j;
    Cache cacheData;
    Cache cacheInstruction;
    FILE *file;
    char mode, address[100];
    
    /* Technically a line shouldn't be longer than 25 characters, but
       allocate extra space in the buffer just in case */
    char buffer[LINELENGTH];

    // calculate other cache parameters

    int argCacheSizeData = argNumCachelinesData * argCachelineSize * argNumWaysData;
    int argCacheSizeInstruction = argNumCachelinesInstrction * argCachelineSize * argNumWaysInstrcution;

    bitsByteSelect = floor(log2(argCachelineSize));
    int bitsIndexData = floor(log2(argNumCachelinesData));
    int bitsIndexInstruction = floor(log2(argNumCachelinesInstrction));

    int bitsTagData = ADDRESS_SIZE - (bitsByteSelect + bitsIndexData);
    int bitsTagInstrction = ADDRESS_SIZE - (bitsByteSelect + bitsIndexInstruction);
    /* Used to handle the onput mode. */
    printmessage = argv[1];
    //printf("P is %s\n", printmessage);

    /* Open the file for reading. */
    file = fopen( argv[2], "r" );
    if( file == NULL )
    {
        fprintf(stderr, "Error: Could not open file.\n");
        return 0; 
    }

    cacheData = createCache(argCacheSizeData, argCachelineSize, write_policy, argNumWaysData, argNumCachelinesData);
    cacheInstruction = createCache(argCacheSizeInstruction, argCachelineSize, write_policy, argNumWaysInstrcution, argNumCachelinesInstrction);

    counter = 1;
    
    while( fgets( buffer, LINELENGTH, file ) != NULL )
    {
        if(buffer[0] != EOF)
        {
            i = 0;
            mode = buffer[i];
            if(mode != '0' && mode != '1' && mode != '2' && mode != '3' && mode != '4' && mode != '8' && mode != '9'){
                if(mode == '5' || mode == '6' || mode == 7){
                    if(PRINTLINE) printf("\nLine %i: Invalid instruction number! Check your input!!\n", counter);
                    counter++;
                    continue;
                }else
                    continue;
            }

            // while(mode == 13|| mode == 10 || mode == 0 || mode == 32){
            //     i++;
            //     mode = buffer[i];
            // }

            i += 2;
            j = 0;
            
            while(buffer[i] != '\0' && buffer[i] != ' ' && buffer[i] != '\n' && buffer[i] !=\
             '\r'  && buffer[i] != '\t')
            {
                address[j] = buffer[i];
                i++;
                j++;
            }
            
            address[j] = '\0';
            // if(mode == 13 || mode == 10 || mode == 0 || mode == 32 || mode == 9\
            //     || mode == 25 || mode == 120){
            //     continue;
            // }
            
            if(mode == '0')
            {
                if(PRINTLINE) printf("\nLine %i:  ReadFromCache -- Address %s\n", counter, address);
                readFromCache(cacheData, address, argNumWaysData, bitsTagData, bitsIndexData);
            }
            else if(mode == '1')
            {
                if(PRINTLINE) printf("\nLine %i: WriteToCache -- Address %s\n", counter, address);
                writeToCache(cacheData, address, argNumWaysData, bitsTagData, bitsIndexData);
            }
            else if(mode == '2'){
                if(PRINTLINE) printf("\nLine %i: InstructionFetch -- Address %s\n", counter, address);
                readFromCache(cacheInstruction, address, argNumWaysInstrcution, bitsTagInstrction, bitsIndexInstruction);
            }
            else if(mode == '3'){
                if(PRINTLINE) printf("\nLine %i: InvalidateCommand -- Address %s\n", counter, address);
                invalidateInstruction(cacheData, address, argNumWaysData, bitsTagData, bitsIndexData);
            }
            else if(mode == '4'){
                if(PRINTLINE) printf("\nLine %i: DataRequest -- Address %s\n", counter, address);
                dataRequest(cacheData, address, argNumWaysData, bitsTagData, bitsIndexData);
            }
            else if(mode == '8'){
                if(PRINTLINE) printf("\nLine %i:  Reset -- Address %s\n", counter, address);
                resetCache(cacheData, argNumCachelinesData, argNumWaysData);
                resetCache(cacheInstruction, argNumCachelinesInstrction, argNumWaysInstrcution);
                cacheData = createCache(argCacheSizeData, argCachelineSize, write_policy, argNumWaysData, argNumCachelinesData);
                cacheInstruction = createCache(argCacheSizeInstruction, argCachelineSize, write_policy, argNumWaysInstrcution, argNumCachelinesInstrction);
            }
            else if(mode == '9'){
                if(PRINTLINE) printf("\nLine %i: Print contents \n", counter);
                
                printf("\nData Cache:\n");
                printCache(cacheData, argNumCachelinesData, argNumWaysData);

                printf("\nInstruction Cache:\n");
                printCache(cacheInstruction, argNumCachelinesInstrction, argNumWaysInstrcution);
            }
            else{
                //continue;
                if(PRINTLINE) printf("\nLine %i: Invalid instruction! Check your input!!\n", counter);
                //printf("Mode is equal to %d, and buffer[0] is %d", mode, buffer[0]);
            }
            counter++;
        }
    }
    printf("\nData Cache:\n");
    printAllCache(cacheData);
    printf("\nInstruction Cache:\n");
    printAllCache(cacheInstruction);
    
    /* Close the file, destroy the cache. */
    printf("\n");
    fclose(file);
    
    destroyCache(cacheData, argNumCachelinesData, argNumWaysData);
    destroyCache(cacheInstruction, argNumCachelinesInstrction, argNumWaysInstrcution);
    //cache = NULL;
    
    return 1;
}

/* createCache
 *
 * Function to create a new cache struct.  Returns the new struct on success
 * and NULL on failure.
 *
 */

Cache createCache(int cache_size, int Cacheline_size, int write_policy, int argNumWays, int argNumCachelines)
{
    /* Local Variables */
    Cache cache;
    int i = 0;
    int j = 0;
    int k = 0;
    
    // allocate all the memory needed for a cache structure
    //printf("Creating a new cache\n");
    cache = (Cache) malloc( sizeof( struct Cache_ ) );
    //printf("Creatinf a new cache finish\n"); 
    if(cache == NULL)
    {
        fprintf(stderr, "Could not allocate memory for cache.\n");
        return NULL;
    }
    
    // take care of most parameters

    cache->hits = 0;
    //printf("First access finish\n"); 
   
    cache->misses = 0;
    cache->reads = 0;
    cache->writes = 0;
    cache->evictions = 0;
    
    cache->write_policy = write_policy;
    
    cache->Cacheline_size = argCachelineSize;
    

    cache->associativity = argNumWays;

    // allocate all the memory for ALL ways
    cache->ways = (Way *) malloc( sizeof(struct Way_) * argNumWays );

    for(j = 0; j < argNumWays; j++) {

        // allocate memory for this INDIVIDUAL way
        cache->ways[j] = (Way) malloc( sizeof( struct Way_ ) );

        cache->ways[j]->waynum = j;

        // allocate space for ALL cachelines
        cache->ways[j]->cachelines = (Cacheline *) malloc( sizeof(struct Cacheline_) * argNumCachelines );

        /* By default insert cachelines in invalid  state */

        for(i = 0; i < argNumCachelines; i++)
        {
            // allocate memory for this INDIVIDUAL Cacheline
            cache->ways[j]->cachelines[i] = (Cacheline) malloc( sizeof( struct Cacheline_ ) );

            // take care of other parameters for this cacheline
            cache->ways[j]->cachelines[i]->mesi = 0;
            cache->ways[j]->cachelines[i]->firstWrite = 1;
            cache->ways[j]->cachelines[i]->tag = NULL;

            // create the linked list for this cacheline
            cache->ways[j]->cachelines[i]->list = malloc(sizeof(struct node));

            cache->ways[j]->cachelines[i]->list->data = 0;
            cache->ways[j]->cachelines[i]->list->next = NULL;

            for (k = 1; k < argNumWays; k++) {
                push(&cache->ways[j]->cachelines[i]->list, k);
            }
        }
    }    
    return cache;
}

/* destroyCache
 * 
 * Function that destroys a created cache. Frees all allocated memory. If 
 * you pass in NULL, nothing happens. So make sure to cacheline your cache = NULL
 * after you destroy it to prevent a double free.
 *
 */

void destroyCache(Cache cache, int argNumCachelines, int argNumWays) 
{
    int i;
    int j;
    
    if(cache != NULL) {

        // deallocate ALL ways in this cache
        for (j = 0; j < argNumWays; j++) {

            // deallocate ALL cachelines in this way
            for( i = 0; i < argNumCachelines; i++ ) {

                // check if the cacheline entry is NULL
                if(cache->ways[j]->cachelines[i]->tag != NULL) {
                    free(cache->ways[j]->cachelines[i]->tag);
                }

                // deallocate this INDIVIDUAL cacheline
                //free(cache->ways[j]->cachelines[i]);
            }

            // deallocate this INDIVIDUAL way
            //free(cache->ways[j]->cachelines);
            free(cache->ways[j]);
        }
        free(cache->ways);
        free(cache);
    }

    return;
}

/* resetCache
 * 
 * Function that resets a created cache. 
 */

void resetCache(Cache cache, int argNumCachelines, int argNumWays) 
{
    int i;
    int j;
    if(cache != NULL) {
        // deallocate ALL ways in this cache
        for (j = 0; j < argNumWays; j++) {
            // deallocate ALL cachelines in this way
            for( i = 0; i < argNumCachelines; i++ ) {
                // check if the Cacheline entry is NULL
                if(cache->ways[j]->cachelines[i]->tag != NULL) {
                    free(cache->ways[j]->cachelines[i]->tag);
                }
                // deallocate the list for LRU
                free(cache->ways[j]->cachelines[i]->list);
                // deallocate this INDIVIDUAL Cacheline
                free(cache->ways[j]->cachelines[i]);
            }

            // deallocate this INDIVIDUAL way
            free(cache->ways[j]->cachelines);
            free(cache->ways[j]);
        }
        free(cache->ways);
        free(cache);
    }

    return;
}

/* readFromCache
 *
 * Function that reads data from a cache. Returns 0 on failure
 * or 1 on success. 
 *
 */

int readFromCache(Cache cache, char* address, int argNumWays, int bitsTag, int bitsIndex)
{
    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *byteSelect;
    int i;
    int j;
    int noHit = 1;
    int LRU = 0;

    Cacheline cacheline;
    /* Convert and parse necessary values */
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring, bitsTag, bitsIndex);
    
    if(DEBUG)
    {
        printf("\tHex: %s\n", address);
        printf("\tDecimal: %u\n", dec);
        printf("\tBinary: %s\n", bstring);
        printf("\tFormatted: %s\n\n", bformatted);
    }
        
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    byteSelect = (char *) malloc( sizeof(char) * (bitsByteSelect + 1) );
    byteSelect[bitsByteSelect] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsByteSelect + bitsIndex + bitsTag + 2; i++)
    {
        byteSelect[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    if(DEBUG)
    {
        printf("\tTag: %s (%i)\n", tag, btoi(tag));
        printf("\tIndex: %s (%i)\n", index, btoi(index));
        printf("\tByteSelect: %s (%i)\n\n", byteSelect, btoi(byteSelect));
    }
    
    /* Get the cacheline */
    
    if(DEBUG) printf("\tAttempting to read data from cache slot %i.\n\n", btoi(index));

    cache->reads++;

    for(j = 0; j < argNumWays; j++) {

        cacheline = cache->ways[j]->cachelines[btoi(index)];

        // if there's a cache hit (valid state && tags match)
        if(cacheline->tag == NULL)
            continue;

        if(cacheline->mesi >= 1 && strcmp(cacheline->tag, tag) == 0) {
            cache->hits++;
            noHit = 0;
            LRU = j;
            free(tag);
        }

    }

    // otherwise, cache miss - implement LRU here
    int t = 0;
    int countForInvalid = 0;

    if (noHit == 1) {

        cache->misses++;
        LRU = findLeast(cacheline->list);
        //printf("lru is %d\n", LRU);
        //Used to check invalid slots
        for(j = 0; j < argNumWays; j++) {
            cacheline = cache->ways[j]->cachelines[btoi(index)];

            if(cacheline->mesi == 0)
                countForInvalid++;
        }
        //printf("countForInvalid %d\n", countForInvalid);
        //Only one invalid slot
        if( countForInvalid >= 1){
            //Mark the slot
            for(j = 0; j < argNumWays; j++) {
                cacheline = cache->ways[j]->cachelines[btoi(index)];
                if(cacheline->mesi == 0){
                    LRU = j; 
                    break;
                }
            }//Do things for this case
            cacheline = cache->ways[LRU]->cachelines[btoi(index)]; 
            if(cacheline->mesi == 0){
                cacheline->mesi = 2;
                if(*printmessage == '1'){  
                    printf("Read from L2 %s\n", address );  
                }        
            }
        }else if(countForInvalid == 0){//Full cache set, print read from L2
            cacheline = cache->ways[LRU]->cachelines[btoi(index)];
            if(cacheline->mesi == 0){
                cacheline->mesi = 2;
                if(*printmessage == '1'){
                    printf("Read from L2 %s\n", address );
                }
            }else if(cacheline->mesi == 1 && findLRU(cacheline->list, LRU) == argNumWays - 1){
            	cacheline->mesi = 2;
                if(*printmessage == '1'){
            	   printf("Write to L2 %s\n", address );
                }
            }
        }else{//Over one invalid slot
            cacheline = cache->ways[LRU]->cachelines[btoi(index)];
            if(cacheline->mesi == 0){
                cacheline->mesi = 2;
                if(*printmessage == '1'){
                    printf("Read from L2 %s\n", address );
                }
                
            }

            if(cacheline->tag != NULL) {
                cache->evictions++;
                //free(cacheline->tag);            
            }

        }

        cacheline->tag = tag;

    }

    //To deal with MESI in read
    // if( countForInvalid != 1)

    for(j = 0; j < argNumWays; j++) {

        UpdateLRU(cache->ways[j]->cachelines[btoi(index)]->list,LRU);
    }

    if (DEBUG) {
        printf("\tLRU:");
        printList(cacheline->list);
        printf("\n");
    }

    // free(bstring);
    // free(bformatted);
    free(byteSelect);
    free(index);
    return 1;
}

/* invalidateInstruction
 *
 * Function for an invalidate command from L2. Returns 0 on failure
 * or 1 on success. 
 *
 */

int invalidateInstruction(Cache cache, char* address, int argNumWays, int bitsTag, int bitsIndex)
{
    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *byteSelect;
    int i = 0;
    int j = 0;
    Cacheline cacheline;
    /* Convert and parse necessary values */
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring, bitsTag, bitsIndex);
    
    if(DEBUG)
    {
        printf("\tHex: %s\n", address);
        printf("\tDecimal: %u\n", dec);
        printf("\tBinary: %s\n", bstring);
        printf("\tFormatted: %s\n\n", bformatted);
    }
        
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    byteSelect = (char *) malloc( sizeof(char) * (bitsByteSelect + 1) );
    byteSelect[bitsByteSelect] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsByteSelect + bitsIndex + bitsTag + 2; i++)
    {
        byteSelect[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    if(DEBUG)
    {
        printf("\tTag: %s (%i)\n", tag, btoi(tag));
        printf("\tIndex: %s (%i)\n", index, btoi(index));
        printf("\tByteSelect: %s (%i)\n\n", byteSelect, btoi(byteSelect));
    }
    
    /* Get the cacheline */
    
    if(DEBUG) printf("\tData request to cache slot %i.\n\n", btoi(index));

    for (j = 0; j < argNumWays; j++) {

        cacheline = cache->ways[j]->cachelines[btoi(index)];

        if(cacheline->tag == NULL){
            //cacheline->tag = tag;
            //free(tag);
            break;
        }
        
        if(strcmp(cacheline->tag, tag) == 0){
        	//cacheline->tag = tag;
        	free(tag);
            if(cacheline->mesi == 3){
                cacheline->mesi = 0;
            }else if(cacheline->mesi == 2) {
                cacheline->mesi = 0;
                //if(*printmessage == '1')
                    //printf("Write to L2 %s\n", address );
            }else if(cacheline->mesi == 1) {
                // if(*printmessage == '1')
                    //printf("Invalid request\n");
            }
        }
    }

    if (DEBUG) {
        printf("\tLRU:");
        printList(cacheline->list);
        printf("\n");
    }

    
    // free(bstring);
    // free(bformatted);
    free(byteSelect);
    free(index);
    return 1;
}

/* dataRequest
 *
 * Function for data request from L2. Returns 0 on failure
 * or 1 on success. 
 *
 */

int dataRequest(Cache cache, char* address, int argNumWays, int bitsTag, int bitsIndex)
{
    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *byteSelect;
    int i = 0;
    int j = 0;
    Cacheline cacheline;
  
    /* Convert and parse necessary values */
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring, bitsTag, bitsIndex);
    
    if(DEBUG)
    {
        printf("\tHex: %s\n", address);
        printf("\tDecimal: %u\n", dec);
        printf("\tBinary: %s\n", bstring);
        printf("\tFormatted: %s\n\n", bformatted);
    }
        
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    byteSelect = (char *) malloc( sizeof(char) * (bitsByteSelect + 1) );
    byteSelect[bitsByteSelect] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsByteSelect + bitsIndex + bitsTag + 2; i++)
    {
        byteSelect[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    if(DEBUG)
    {
        printf("\tTag: %s (%i)\n", tag, btoi(tag));
        printf("\tIndex: %s (%i)\n", index, btoi(index));
        printf("\tByteSelect: %s (%i)\n\n", byteSelect, btoi(byteSelect));
    }
    
    /* Get the cacheline */
    
    if(DEBUG) printf("\tAttempting to handle data request from L2 %i.\n\n", btoi(index));


    for (j = 0; j < argNumWays; j++) {

        cacheline = cache->ways[j]->cachelines[btoi(index)];

        if(cacheline->tag == NULL){
            // cacheline->tag = tag;
            break;
        }

        if( strcmp(cacheline->tag, tag) == 0){
            free(tag);
            if(cacheline->mesi == 1){
                cacheline->mesi = 2;
                if(*printmessage == '1')
                    printf("Return data to L2 %s\n", address );
            }else if(cacheline->mesi == 3){
                cacheline->mesi = 2;
            }
            else {
                // if(*printmessage == '1')
                    //printf("Invalid request\n");
            }           
        }
    }
    if (DEBUG) {
        printf("\tLRU:");
        printList(cacheline->list);
        printf("\n");
    }

    // free(bstring);
    // free(bformatted);
    free(byteSelect);
    free(index);
    return 1;
}

/* writeToCache
 *
 * Function that writes data to the cache. Returns 0 on failure or
 * 1 on success. Frees any old tags that already existed in the
 * target slot.
 */

int writeToCache(Cache cache, char* address, int argNumWays, int bitsTag, int bitsIndex)
{
    unsigned int dec;
    char *bstring, *bformatted, *tag, *index, *byteSelect;
    int i = 0;
    int j = 0;
    Cacheline cacheline;
    int noHit = 1;
    int LRU = 0;
  
    /* Convert and parse necessary values */
    
    dec = htoi(address);
    bstring = getBinary(dec);
    bformatted = formatBinary(bstring, bitsTag, bitsIndex);
    
    if(DEBUG)
    {
        printf("\tHex: %s\n", address);
        printf("\tDecimal: %u\n", dec);
        printf("\tBinary: %s\n", bstring);
        printf("\tFormatted: %s\n\n", bformatted);
    }
        
    i = 0;
    
    tag = (char *) malloc( sizeof(char) * (bitsTag + 1) );
    tag[bitsTag] = '\0';
    
    for(i = 0; i < bitsTag; i++)
    {
        tag[i] = bformatted[i];
    }
    
    index = (char *) malloc( sizeof(char) * (bitsIndex + 1) );
    index[bitsIndex] = '\0';
    
    for(i = bitsTag + 1; i < bitsIndex + bitsTag + 1; i++)
    {
        index[i - bitsTag - 1] = bformatted[i];
    }
    
    byteSelect = (char *) malloc( sizeof(char) * (bitsByteSelect + 1) );
    byteSelect[bitsByteSelect] = '\0';
    
    for(i = bitsIndex + bitsTag + 2; i < bitsByteSelect + bitsIndex + bitsTag + 2; i++)
    {
        byteSelect[i - bitsIndex - bitsTag - 2] = bformatted[i];
    }
    
    if(DEBUG)
    {
        printf("\tTag: %s (%i)\n", tag, btoi(tag));
        printf("\tIndex: %s (%i)\n", index, btoi(index));
        printf("\tByteSelect: %s (%i)\n\n", byteSelect, btoi(byteSelect));
    }
    
    /* Get the cacheline */
    
    if(DEBUG) printf("\tAttempting to write data to cache slot %i.\n\n", btoi(index));

    cache->writes++;

    for (j = 0; j < argNumWays; j++) {

        cacheline = cache->ways[j]->cachelines[btoi(index)];

        if(cacheline->tag == NULL)
            continue;

        if(cacheline->mesi >= 1 && strcmp(cacheline->tag, tag) == 0) {

            noHit = 0;
            //cacheline->dirty = 1;
            cache->hits++;
            free(tag);
            LRU = j;
            if(cacheline->mesi == 3){
                cacheline->mesi = 1;
            }else if(cacheline->mesi == 2) {
                cacheline->mesi = 3;
                if(*printmessage == '1'){
                    printf("Write to L2 %s\n", address );
                }
                
            }else if(cacheline->mesi == 0) {
                cacheline->mesi = 3;
                if(*printmessage == '1'){
                    printf("Read For Ownership from L2 %s\n", address );
                    printf("Write to L2 %s\n", address );
                }
            }
        }
    }

    int t = 0;
    int countForInvalid = 0;

    if (noHit == 1) {

        cache->misses++;
        LRU = findLeast(cacheline->list);
        //Used to check invalid slots
        for(j = 0; j < argNumWays; j++) {
            cacheline = cache->ways[j]->cachelines[btoi(index)];

            if(cacheline->mesi == 0)
                countForInvalid++;
        }
        //Only one invalid slot
        if( countForInvalid >= 1){
            //Mark the slot
            for(j = 0; j < argNumWays; j++) {
                cacheline = cache->ways[j]->cachelines[btoi(index)];
                if(cacheline->mesi == 0){
                    LRU = j; 
                    break;
                }
            }//Do things for this case
            cacheline = cache->ways[LRU]->cachelines[btoi(index)]; 
            if(cacheline->mesi == 3){
                cacheline->mesi = 1;
            }else if(cacheline->mesi == 2) {
                cacheline->mesi = 3;
                if(*printmessage == '1')
                    printf("Write to L2 %s\n", address );
            }else if(cacheline->mesi == 0) {
                cacheline->mesi = 3;
                if(*printmessage == '1'){
                    printf("Read For Ownership from L2 %s\n", address );
                    printf("Write to L2 %s\n", address );
                }
            } 
        }else if(countForInvalid == 0){//Full cacheline, print read from L2
            cacheline = cache->ways[LRU]->cachelines[btoi(index)];
            if(cacheline->mesi == 3){
                cacheline->mesi = 1;
                if(*printmessage == '1'){
                    printf("Read For Ownership from L2 %s\n", address );
                }
            }else if(cacheline->mesi == 2) {
                cacheline->mesi = 3;
                if(*printmessage == '1')
                    printf("Write to L2 %s\n", address );
            }else if(cacheline->mesi == 0) {
                cacheline->mesi = 3;
                if(*printmessage == '1'){
                    printf("Read For Ownership from L2 %s\n", address );
                    printf("Write to L2 %s\n", address );
                }
            
            }else if(cacheline->mesi == 1 && findLRU(cacheline->list, LRU) == argNumWays - 1){
            	cacheline->mesi = 3;
                if(*printmessage == '1'){
            	   printf("Write to L2 %s\n", address );
            	   printf("Read For Ownership from L2 %s\n", address );
                   printf("Write to L2 %s\n", address );
               }
            }else {
                if(*printmessage == '1'){
                    printf("Write to L2 %s\n", address );

                }
            }

        }else{//Over one invalid slot
            cacheline = cache->ways[LRU]->cachelines[btoi(index)];
            if(cacheline->mesi == 3){
                cacheline->mesi = 1;
            }else if(cacheline->mesi == 2) {
                cacheline->mesi = 3;
                if(*printmessage == '1')
                    printf("Write to L2 %s\n", address );
            }else if(cacheline->mesi == 0) {
                cacheline->mesi = 3;
                if(*printmessage == '1'){
                    printf("Read For Ownership from L2 %s\n", address );
                    printf("Write to L2 %s\n", address );
                }
            } 

        }

        cacheline->tag = tag;

    }
    


    for(j = 0; j < argNumWays; j++) {

        UpdateLRU(cache->ways[j]->cachelines[btoi(index)]->list,LRU);
    }

    if (DEBUG) {
        printf("\tLRU:");
        printList(cacheline->list);
        printf("\n");
    }

    // free(bstring);
    // free(bformatted);
    free(byteSelect);
    free(index);
    return 1;
}

/* printCache
 *
 * Prints out the values of each slot in the cache
 * as well as the hit, miss, read, write, and size
 * data.
 */

void printCache(Cache cache, int argNumCachelines, int argNumWays)
{
    int i;
    int j;

    int cache_total = (int) (cache->hits) + (cache->misses);
    char* tag;
    
    if(cache != NULL) {
        int u;
        for (i = 0; i < argNumCachelines; i++) {

            //printf("\n\n******** Way # %d ********\n\n", cache->ways[j]->waynum);
            for(j = 0; j < argNumWays; j++) {
                //tag = "NULL";

                if(cache->ways[j]->cachelines[i]->tag != NULL) {
                    u = i;
                    tag = cache->ways[j]->cachelines[i]->tag;
                    if(j == 0){
                        printf("Set[%i]:", i);
                        //printList(cache->ways[j]->cachelines[i]->list);   
                        printf("\n");                         
                    }
                    //printf("\n");
                    printf("\tWay # %d MESI: %c",cache->ways[j]->waynum, findMESI(cache->ways[j]->cachelines[i]));
                    
                    int value = (int)strtol(tag, NULL, 2);
                    char hexString[16];
                    sprintf(hexString, "%x", value);

                    printf("\ttag: %s",  hexString);
                    printf("\tLRU:");
                    printLRU(cache->ways[j]->cachelines[i]->list, j);
                    printf("\n");

                }
            }
            if (i == u + 1){
                printf("\n");
            }
        }

        // if(printmessage){
        //     printf("\nCache parameters:\n\n");
        //     printf("\tCache size: %d\n", cache->cache_size);
        //     printf("\tCache Cacheline size: %d\n", cache->Cacheline_size);
        //     printf("\tCache number of lines: %d\n", argNumCachelines);
        //     printf("\tCache associativity: %d\n", argNumWays);
        // }

        

        // //printf("\nCache performance:\n\n");
        // printf("\tCache reads: %d\n", cache->reads);
        // printf("\tCache writes: %d\n", cache->writes);
        // printf("\tCache hits: %d\n", cache->hits);
        // printf("\tCache misses: %d\n", cache->misses);
        // //printf("\tCache total access: %d\n\n", cache_total);

        // printf("\tCache hit ratio: %2.2f%%\n", ((float) (cache->hits) / (float) (cache_total) ) * 100);
        // //printf("\tCache miss ratio: %2.2f%%\n\n", ((float) (cache->misses) / (float) (cache_total) ) * 100);



        // //printf("\tCache evictions: %d\n", cache->evictions);

    }
}

void printAllCache(Cache cache)
{

    int cache_total = (int) (cache->hits) + (cache->misses);

    
    if(cache != NULL) {

    //printf("\nCache performance:\n\n");
    printf("\tCache reads: %d\n", cache->reads);
    printf("\tCache writes: %d\n", cache->writes);
    printf("\tCache hits: %d\n", cache->hits);
    printf("\tCache misses: %d\n", cache->misses);
    //printf("\tCache total access: %d\n\n", cache_total);

    printf("\tCache hit ratio: %2.2f%%\n", ((float) (cache->hits) / (float) (cache_total) ) * 100);
    //printf("\tCache miss ratio: %2.2f%%\n\n", ((float) (cache->misses) / (float) (cache_total) ) * 100);



    //printf("\tCache evictions: %d\n", cache->evictions);

    }
}

void UpdateLRU(struct node *head, int value)
{
    int j = 0;
    int i = 0;
    int t = 0;
    struct node* n = head;

    while (n != NULL) {
        if(i == value){
            t = n->data;
            n->data = 0;
            break;
        }
        n = n->next;
        i++;
    }
    n = head;
    while (n != NULL) {
        if(n->data < t && j != i){
            n->data = n->data + 1;
        }
        n = n->next;
        j++;
    }
    return;
}

void deleteNode(struct node *head, int value)
{
    struct node* n = head;

    while (n->data != value && n != NULL) {
        n = n->next;
    }

    // When node to be deleted is head node
    if(head == n)
    {
        if(head->next == NULL)
        {
            if (DEBUG) printf("There is only one node. The list can't be made empty ");
            return;
        }

        /* Copy the data of next node to head */
        head->data = head->next->data;

        // store address of next node
        n = head->next;

        // Remove the link of next node
        head->next = head->next->next;

        // free memory
        free(n);

        return;
    }


    // When not first node, follow the normal deletion process

    // find the previous node
    struct node *prev = head;
    while(prev->next != NULL && prev->next != n)
        prev = prev->next;

    // Check if node really exists in Linked List
    if(prev->next == NULL)
    {
        printf("\n Given node is not present in Linked List");
        return;
    }

    // Remove node from Linked List
    prev->next = prev->next->next;

    // Free memory
    free(n);

    return;
}

/* Utility function to insert a node at the begining */
void push(struct node **head_ref, int new_data)
{
    struct node *new_node = (struct node *)malloc(sizeof(struct node));
    new_node->data = new_data;
    new_node->next = *head_ref;
    *head_ref = new_node;
}

/* Utility function to print a linked list */
void printList(struct node *head)
{
    //printf("\tLinked list LRU:  ");

    while(head!=NULL)
    {
        printf(" ");
        printf("%d ",head->data);
        head=head->next;
    }
    //printf("\n");
}

/* Utility function to print a linked list */
void printLRU(struct node *head, int i)
{
    //printf("\tLinked list LRU:  ");
    int j = 0;
    while(head!=NULL)
    {
        // printf(" ");
        
        j++;
        if(j == i + 1){
            printf("%d ",head->data);
        }
        head=head->next;
    }
    //printf("\n");
}

/* Utility function to find the LRU bit for a perticular way(input) */
int findLRU(struct node *head, int i)
{
    //printf("\tLinked list LRU:  ");
    int j = 0;
    while(head!=NULL)
    {
        // printf(" ");
        
        j++;
        if(j == i + 1){
            break;
        }
        head=head->next;
    }
    return head->data;
}

int findLast(struct node* head) {

    while(head->next!= NULL) {
        head = head->next;
    }
    return head->data;
}

//Find the way # of the LRU
int findLeast(struct node* head) {
    int t = 0;
    int j = 0;
    struct node* n = head;
    while(n!= NULL) {
        if(n->data > t){
            t = n->data;
        }
        n = n->next;
    }
    while(head->next!= NULL) {
        if(head->data == t){
            break;     
        }
        j++;
        head = head->next;
    }
    return j;
}

char findMESI(Cacheline cacheline){
    if( cacheline->mesi == 0 )
        return 'I';
    else if(cacheline->mesi == 1)
        return 'M';
    else if(cacheline->mesi == 2)
        return 'S';
    else
        return 'E';
}

